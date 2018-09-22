.PHONY : all

HOSTNAME := $(shell hostname -s)

ifeq ($(HOSTNAME), incepchow)
	BLENDER = ~/blender-2.79b-linux-glibc219-x86_64/blender
else ifeq ($(HOSTNAME), retchow)
	BLENDER=/Applications/Blender/blender.app/Contents/MacOS/blender
else
	BLENDER = blender
endif

DIST=../dist

all : \
	$(DIST)/menu.p \
	$(DIST)/tank.pnc \
	$(DIST)/tank.scene \

$(DIST)/%.p : %.blend export-meshes.py
	$(BLENDER) --background --python export-meshes.py -- '$<' '$@'

$(DIST)/%.pnc : %.blend export-meshes.py
	$(BLENDER) --background --python export-meshes.py -- '$<' '$@'

# Use layer 2 for scene walkmeshes (add an override if needed)
$(DIST)/%.w : %.blend export-walkmeshes.py
	$(BLENDER) --background --python export-walkmeshes.py -- '$<':2 '$@'

$(DIST)/%.scene : %.blend export-scene.py
	$(BLENDER) --background --python export-scene.py -- '$<' '$@'
