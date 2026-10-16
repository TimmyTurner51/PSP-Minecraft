# Vanilla mob assets on hand

Extracted from `C:\Users\minec\AppData\Roaming\.minecraft\versions\1.7.3\1.7.3.jar`.

25 mobs, 42 textures. Models: chicken, pig.

| mob | textures | model | mesh bounds (px) | sha256 |
|---|---|---|---|---|
| bat | bat.png | - | - | `c465f1b47b784984` |
| blaze | blaze.png | - | - | `d5c1b3185f250648` |
| chicken | chicken.png | ModelChicken | 12 x 15 x 8 | `41d7b04625b1f6e5` |
| cow | cow.png (+1) | - | - | `55c4eb830e310c6c` |
| creeper | creeper.png (+1) | - | - | `b11970ef11ad428f` |
| enderdragon | dragon.png | - | - | `f673a21e0d77ccd7` |
| enderman | enderman.png (+1) | - | - | `de809767be0117af` |
| ghast | ghast.png (+1) | - | - | `b87a7c1e53a355a5` |
| iron_golem | iron_golem.png | - | - | `7ab9fcc8573ac31a` |
| magma_cube | magmacube.png | - | - | `6a8a6fb9fe8185c8` |
| ocelot | black.png (+3) | - | - | `4cac187aa0cf1bcc` |
| pig | pig.png (+1) | ModelPig | 24 x 16 x 10 | `c6bcc41d48803df2` |
| player | steve.png | - | - | `28f71997dfd841d8` |
| sheep | sheep.png (+1) | - | - | `c8424e91f13c19f3` |
| silverfish | silverfish.png | - | - | `381709ce778b7976` |
| skeleton | skeleton.png (+1) | - | - | `4ca6d5128c1879d0` |
| slime | slime.png | - | - | `a72dfd1efd7217a9` |
| snowman | snowman.png | - | - | `1ff3a4c6ef89dbdf` |
| spider | cave_spider.png (+2) | - | - | `0e304d8df4e0b67e` |
| squid | squid.png | - | - | `160c5e20cb884b47` |
| witch | witch.png | - | - | `cfcc63cfb7c1e0c5` |
| wither | wither.png (+2) | - | - | `3aa224a6a9642269` |
| wolf | wolf.png (+3) | - | - | `53d7a333f011baba` |
| zombie | zombie.png | - | - | `5e1b60687d0f33cb` |
| zombie_pigman | zombie_pigman.png | - | - | `bbf2931aabf1f446` |

`mesh bounds` is the model's own extent (beak, legs and all) in model px, 1 px = 1/16 block. It is not vanilla's entity hitbox -- that lives in `EntityX.setSize` and is not invented here.
