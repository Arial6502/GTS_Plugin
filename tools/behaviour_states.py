"""Fills in ActionDef::State from a Nemesis behaviour project.

For every action in the node tables under src/Actions/Nodes, finds the behaviour graph states that
have a transition on the action's event and writes them into the row as .State = "A|B".

    python tools/behaviour_states.py "<mod>/Nemesis_Engine/mod/gtss"          report only
    python tools/behaviour_states.py "<mod>/Nemesis_Engine/mod/gtss" apply    edit the tables
    python tools/behaviour_states.py "<mod>/Nemesis_Engine/mod/gtss" refresh  replace states already set

Run it from the repository root. An action is left alone when its event is a wildcard transition,
when the transition belongs to a vanilla state the mod does not modify, or when no transition
exists. Event names are compared without case, since the project and the code spell them differently.
"""
import collections
import glob
import os
import re
import sys

PROJECTS = ['0_master', 'mt_behavior', '1hm_behavior', 'magicbehavior', '1hm_locomotion']

# Sent by the node itself, so the row stays ungated.
SKIP = {'Sandwich.Butt.Finisher'}


def param(text, name):
    found = re.search(r'<hkparam name="%s"[^>]*>(.*?)</hkparam>' % name, text, re.S)
    return found.group(1).strip() if found else None


def event_name(raw):
    found = re.match(r'\$eventID\[(.*)\]\$', raw)
    return (found.group(1) if found else raw).lower()


def read_project(folder, origins, wildcards, orphans):
    objects = {}

    for path in glob.glob(os.path.join(folder, '*.txt')):
        text = open(path, encoding='utf-8', errors='replace').read()
        found = re.search(r'<hkobject name="([^"]+)" class="([^"]+)"', text)
        if found:
            objects[found.group(1)] = (found.group(2), text)

    def events(ref):
        if ref not in objects:
            return []
        return [event_name(e) for e in re.findall(r'<hkparam name="eventId">(.*?)</hkparam>', objects[ref][1])]

    used = set()

    for kind, text in objects.values():
        if kind == 'hkbStateMachine':
            ref = param(text, 'wildcardTransitions')
            if ref and ref != 'null':
                used.add(ref)
                for event in events(ref):
                    wildcards[event].add(param(text, 'name'))
        elif kind == 'hkbStateMachineStateInfo':
            ref = param(text, 'transitions')
            if ref and ref != 'null':
                used.add(ref)
                for event in events(ref):
                    origins[event].add(param(text, 'name'))

    # A transition array no state in this folder points at belongs to an unmodified vanilla state.
    for key, (kind, _) in objects.items():
        if kind == 'hkbStateMachineTransitionInfoArray' and key not in used:
            orphans.update(events(key))


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 1

    root = sys.argv[1]
    mode = sys.argv[2] if len(sys.argv) > 2 else 'report'

    origins = collections.defaultdict(set)
    wildcards = collections.defaultdict(set)
    orphans = set()

    for project in PROJECTS:
        read_project(os.path.join(root, project), origins, wildcards, orphans)

    constants = {}
    for path in glob.glob('src/Actions/**/*.?pp', recursive=True):
        text = open(path, encoding='utf-8', errors='replace').read()
        for found in re.finditer(r'constexpr std::string_view (\w+)\s*=\s*"([^"]+)"', text):
            constants[found.group(1)] = found.group(2)

    for path in sorted(glob.glob('src/Actions/Nodes/**/*Node.cpp', recursive=True)):
        raw = open(path, 'rb').read().decode('utf-8', errors='surrogateescape')
        table = re.search(r'constexpr ActionDef \w+\[\] = \{(.*?)\n\t\};', raw, re.S)

        if not table:
            continue

        print('##', os.path.basename(path))
        block = table.group(1)
        edited = block

        for row in re.finditer(r'(\{ \.Action = "([^"]+)",\s*\.Behaviour = ([^,}]+),)( \.State = (\w+|"[^"]*"),)?', block):
            action = row.group(2)
            behaviour = row.group(3).strip()
            behaviour = behaviour.strip('"') if behaviour.startswith('"') else constants.get(behaviour.split('::')[-1], '')
            key = behaviour.lower()

            if not behaviour:
                continue

            reason = None
            if row.group(4) and mode != 'refresh':
                reason = 'already set'
            elif action in SKIP:
                reason = 'sent by the node'
            elif key in wildcards:
                reason = 'wildcard in ' + ','.join(sorted(wildcards[key]))
            elif key in orphans:
                reason = 'transition in an unmodified vanilla state'
            elif not origins.get(key):
                reason = 'no transition found'

            if reason:
                print('  skip %-24s %-32s %s' % (action, behaviour, reason))
                continue

            states = '|'.join(sorted(origins[key]))
            print('  gate %-24s %s' % (action, states))
            edited = edited.replace(row.group(0), '%s .State = "%s",' % (row.group(1), states), 1)

        if mode in ('apply', 'refresh') and edited != block:
            open(path, 'wb').write(raw.replace(block, edited, 1).encode('utf-8', errors='surrogateescape'))

    return 0


if __name__ == '__main__':
    sys.exit(main())
