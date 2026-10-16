#!/bin/sh
set -e
cd /src/third_party/mm
OBJDIR=/src/third_party/mm/build/ee/obj_o2
SELF=/src/scripts/compile_ee_o2.sh
FILES="
src/code/z_scene_table.c
src/overlays/actors/ovl_En_Tru_Mt/z_en_tru_mt.c
src/overlays/actors/ovl_En_Twig/z_en_twig.c
src/overlays/actors/ovl_En_Viewer/z_en_viewer.c
src/overlays/actors/ovl_En_Vm/z_en_vm.c
src/overlays/actors/ovl_En_Trt/z_en_trt.c
src/overlays/actors/ovl_En_Trt2/z_en_trt2.c
"
for f in $FILES; do
  rm -f "$OBJDIR/${f%.c}.o"
  sh "$SELF" --one "$f"
done
