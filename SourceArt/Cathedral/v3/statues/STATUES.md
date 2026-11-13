# Statues for the cathedral front (to generate in Meshy)

The eight niches on the main front get one statue each. The pictures in this folder are cut from
`Cathedral images/Front_statues_closeup.JPG`, `Front_closeup_3.jpg` and `Front_statues_closeup_2.jpg`.
They are small, so give Meshy the picture **and** the prompt; the prompt carries most of the detail.

## Meshy settings
- One statue per generation, so each gets its own full texture.
- Image to 3D with the crop, plus the prompt below. If a result looks wrong, try Text to 3D with the prompt alone.
- Symmetry: off. Pose: as in the picture (not a T-pose).
- Texture: standard (no baked lighting or ray tracing), 2K.
- Export: FBX with textures.
- Save each download into this folder, in a subfolder named like the picture (`02_door_right/` and so on).
  I import them, scale them to the niche and place them.

Add this to every prompt: *weathered pale grey limestone, Baroque church statue, full figure,
standing on a small rough rock base, front view, plain background, no niche, no wall.*

## Niche statues
| File | Niche | Height in game | Prompt |
|---|---|---|---|
| `01_door_left.png` | Left of the main door | 1.75 m | Stone statue of Saint Peter, an old bearded apostle in a long robe and cloak, holding two large keys in his right hand and a book in his left |
| `02_door_right.png` | Right of the main door | 1.75 m | Stone statue of Saint Paul, a bald bearded apostle in a long robe, resting his right hand on a tall sword that stands point-down, holding a book against his chest |
| `03_middle_left.png` | Middle row, left | 1.6 m | Stone statue of a bearded saint in a long tunic and cloak, holding a tall palm branch upright in his right hand, left hand open at his side |
| `04_middle_centre.png` | Middle row, centre | 1.6 m | Stone statue of the Virgin Mary seated, wearing a veil and a wide draped robe, holding the Christ child on her left knee |
| `05_middle_right.png` | Middle row, right | 1.6 m | Stone statue of a bearded saint in a long tunic with a cloak over his left shoulder, right hand on his chest, left hand holding a book at his hip |
| `06_top_left.png` | Top row, left | 1.5 m | Stone statue of a young saint with a halo of rays behind his head, in deacon's robes, holding an open book in front of his chest |
| `07_top_centre.png` | Top row, centre | 1.5 m | Stone statue of a figure in swirling Baroque drapery rising on clouds, arms open, head turned upward |
| `08_top_right.png` | Top row, right | 1.5 m | Stone statue of a bishop wearing a tall mitre and a cope, right hand raised in blessing, left hand holding a bishop's crozier |

## Optional pieces
| File | Place | Size in game | Prompt |
|---|---|---|---|
| `09_window_left.png` | Sitting on the scroll left of the round window | 1.4 m | Stone statue of a seated woman in heavy Baroque drapery, turned to her right, one arm raised |
| `10_window_right.png` | Sitting on the scroll right of the round window | 1.4 m | Stone statue of a seated woman in heavy Baroque drapery, turned to her left, leaning on one arm |
| `11_cherub_head.png` | Under the centre niche of the middle row | 0.45 m | Stone carving of a winged cherub head, chubby child's face with curly hair, two small feathered wings behind, flat back to fix on a wall |

Until a statue is delivered its niche stays empty, with the pedestal ready.

## Places added on 2026-11-12 (no crops: the photos do not show these figures closely enough)
These are ready in the model and stay as they are until a model is delivered. Use Text to 3D with the prompt.

| Place | Count | Size in game | Prompt |
|---|---|---|---|
| Left side door, the two shell niches between the columns (`SM_Cathedral_SideL_doorway`) | 2 | 1.55 m | Stone statue of a robed saint holding a book, weathered pale grey limestone, full figure on a small rough rock base, front view, no niche, no wall |
| Gargoyles under the buttress tops on both long sides (now simple angular beasts built from code) | 6 | 1.35 m long | Gothic stone gargoyle water spout, a lean winged beast stretching forward with open jaws, carved from one block of weathered limestone, flat square back to fix on a wall |
| Bell tower, on top of the lantern (now an iron wind vane with an arrow) | 1 | 2.0 m | Bronze weathervane statue of an angel with spread wings holding a cross, green patina, standing on a small ball, full figure |

## Places added on 2026-11-12, evening
| File | Place | Count | Size in game | Prompt |
|---|---|---|---|---|
| `12_gargoyle_porch_right.png`, `13_gargoyle_porch_left.png` | On the porch wall above the Gothic side door, beside the arch (cut from `Right_door.jpg` and `Left_door.jpg`) | 2 to 4 | 0.7 m tall | Small Gothic stone gargoyle crouching upright on a ledge, a squat hunched beast with a round head, folded arms and a short body, weathered limestone, flat back to fix on a wall |
| none (photos show the niches empty) | The twelve niches in the jambs of the Gothic side door (`SM_Cathedral_SideR_portal`), six a side, each with a corbel and a canopy | 12 | 1.4 m tall, 0.5 m wide | Gothic stone statue of a standing apostle in long robes, holding a book, weathered pale limestone, slender figure, full figure on a small base, front view, no niche, no wall |

The six buttress gargoyles are now carved beasts built from code (crouching, horns, folded wings, open jaws). They can stay or be swapped for Meshy ones with the prompt above.
