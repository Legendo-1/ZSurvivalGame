"""Read-only: counts the open level's actors by label prefix (digits stripped) and class, to find the houses."""
import unreal, re, collections

world = unreal.EditorLevelLibrary.get_editor_world()
print("LEVEL", world.get_path_name())
actors = unreal.EditorLevelLibrary.get_all_level_actors()
cnt = collections.Counter()
for a in actors:
    lab = a.get_actor_label()
    pre = re.sub(r"[_ ]?\d+$", "", lab)
    cnt[(pre, a.get_class().get_name())] += 1
for (pre, cls), n in sorted(cnt.items(), key=lambda x: -x[1])[:80]:
    print(f"{n:5d}  {cls:28s} {pre}")
print("TOTAL", len(actors))
