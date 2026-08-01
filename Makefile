# tikukits is a library, not a build target: the sources are pulled into
# whichever image wants them by the top-level TikuOS Makefile.  This file
# exists for the checks that have to be run from inside this repo.

TIKU_DIR ?= ..

# Comment style, against the mainline rules.
#
# A plain `make lint` in the tikuOS tree does NOT cover this repo: its scope
# is what that repo tracks, and this one is submodule content there, skipped
# by SKIP_DIRS.  --root points the checker at this tree so it is scoped by
# this repo's own tracking instead.
#
# Note that an UNCOMMITTED file is out of scope too, and silently -- git has
# to know about a file before the checker will open it.  Stage new sources
# before trusting a clean run.
lint:
	@python3 $(TIKU_DIR)/tools/check_comment_style.py --root=$(CURDIR)

.PHONY: lint
