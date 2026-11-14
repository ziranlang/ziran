# Invoke from the repository root, under build-queue.
IMAGE_THUMBNAIL_DIR := $(BUILD_DIR)/tests/image-thumbnail
IMAGE_THUMBNAIL_COMPILER := $(abspath $(BIN_DIR)/ziran)
IMAGE_THUMBNAIL_ARGS := --root tests --module-path $(abspath std)
IMAGE_THUMBNAIL_SOURCE := tests/image_thumbnail_test.zi

.PHONY: image-thumbnail-test image-thumbnail-suite
image-thumbnail-test: $(BIN_DIR)/ziran $(BIN_DIR)/zi2zx
	mkdir -p $(IMAGE_THUMBNAIL_DIR)
	env -u DISPLAY -u WAYLAND_DISPLAY LDLIBS='-lz -lm' $(IMAGE_THUMBNAIL_COMPILER) build --target=c --exe --entry image_thumbnail_test:main $(IMAGE_THUMBNAIL_ARGS) -o $(IMAGE_THUMBNAIL_DIR)/c $(IMAGE_THUMBNAIL_SOURCE)
	env -u DISPLAY -u WAYLAND_DISPLAY $(IMAGE_THUMBNAIL_DIR)/c/image_thumbnail_test
	env -u DISPLAY -u WAYLAND_DISPLAY $(IMAGE_THUMBNAIL_COMPILER) ir --entry image_thumbnail_test:main $(IMAGE_THUMBNAIL_ARGS) -o $(IMAGE_THUMBNAIL_DIR)/ir $(IMAGE_THUMBNAIL_SOURCE)
	env -u DISPLAY -u WAYLAND_DISPLAY LDLIBS='-lz -lm' $(IMAGE_THUMBNAIL_COMPILER) build --target=c --exe --entry image_thumbnail_test:main --root $(IMAGE_THUMBNAIL_DIR)/ir --module-path $(IMAGE_THUMBNAIL_DIR)/ir -o $(IMAGE_THUMBNAIL_DIR)/saved $(IMAGE_THUMBNAIL_DIR)/ir/image_thumbnail_test.zir
	env -u DISPLAY -u WAYLAND_DISPLAY $(IMAGE_THUMBNAIL_DIR)/saved/image_thumbnail_test
	env -u DISPLAY -u WAYLAND_DISPLAY $(IMAGE_THUMBNAIL_COMPILER) bundle --entry image_thumbnail_test:Portable $(IMAGE_THUMBNAIL_ARGS) -o $(IMAGE_THUMBNAIL_DIR)/source.zx $(IMAGE_THUMBNAIL_SOURCE)
	test "$$(env -u DISPLAY -u WAYLAND_DISPLAY $(IMAGE_THUMBNAIL_COMPILER) run $(IMAGE_THUMBNAIL_DIR)/source.zx)" = 0
	test "$$(env -u DISPLAY -u WAYLAND_DISPLAY $(BIN_DIR)/zi2zx run $(IMAGE_THUMBNAIL_DIR)/source.zx)" = 0
	env -u DISPLAY -u WAYLAND_DISPLAY $(IMAGE_THUMBNAIL_COMPILER) bundle --entry image_thumbnail_test:Portable --root $(IMAGE_THUMBNAIL_DIR)/ir --module-path $(IMAGE_THUMBNAIL_DIR)/ir -o $(IMAGE_THUMBNAIL_DIR)/saved.zx $(IMAGE_THUMBNAIL_DIR)/ir/image_thumbnail_test.zir
	test "$$(env -u DISPLAY -u WAYLAND_DISPLAY $(IMAGE_THUMBNAIL_COMPILER) run $(IMAGE_THUMBNAIL_DIR)/saved.zx)" = 0
	test "$$(env -u DISPLAY -u WAYLAND_DISPLAY $(BIN_DIR)/zi2zx run $(IMAGE_THUMBNAIL_DIR)/saved.zx)" = 0
	env -u DISPLAY -u WAYLAND_DISPLAY $(IMAGE_THUMBNAIL_COMPILER) build --target=go --pkg main --exe --entry image_thumbnail_test:Portable $(IMAGE_THUMBNAIL_ARGS) -o $(IMAGE_THUMBNAIL_DIR)/go $(IMAGE_THUMBNAIL_SOURCE)
	env -u DISPLAY -u WAYLAND_DISPLAY GO111MODULE=off go run $(IMAGE_THUMBNAIL_DIR)/go/*.go
	env -u DISPLAY -u WAYLAND_DISPLAY $(IMAGE_THUMBNAIL_COMPILER) build --target=go --pkg main --exe --entry image_thumbnail_test:Portable --root $(IMAGE_THUMBNAIL_DIR)/ir --module-path $(IMAGE_THUMBNAIL_DIR)/ir -o $(IMAGE_THUMBNAIL_DIR)/saved-go $(IMAGE_THUMBNAIL_DIR)/ir/image_thumbnail_test.zir
	env -u DISPLAY -u WAYLAND_DISPLAY GO111MODULE=off go run $(IMAGE_THUMBNAIL_DIR)/saved-go/*.go
	@echo 'std/image: thumbnail C source/saved IR, Go and portable source/saved IR passed'

# Keep the existing image suite's disposable mktemp output in this checkout.
image-thumbnail-suite: image-thumbnail-test
	mkdir -p $(IMAGE_THUMBNAIL_DIR)/tmp
	env -u DISPLAY -u WAYLAND_DISPLAY TMPDIR=$(abspath $(IMAGE_THUMBNAIL_DIR)/tmp) sh tests/std_image_linux.sh $(IMAGE_THUMBNAIL_COMPILER)
	env -u DISPLAY -u WAYLAND_DISPLAY TMPDIR=$(abspath $(IMAGE_THUMBNAIL_DIR)/tmp) sh tests/std_image_transpose.sh $(IMAGE_THUMBNAIL_COMPILER)
	env -u DISPLAY -u WAYLAND_DISPLAY TMPDIR=$(abspath $(IMAGE_THUMBNAIL_DIR)/tmp) sh tests/std_image_composite_at.sh $(IMAGE_THUMBNAIL_COMPILER)
