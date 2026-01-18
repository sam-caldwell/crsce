# Makefile.d/hooks.mk
# (c) 2026 Sam Caldwell. See LICENSE.txt for details
#
# Set up symlinks from .git/hooks to git-hooks so that each file
# in git-hooks/ appears as a hook in .git/hooks/.

.PHONY: hooks
hooks:
	@echo "--- Linking git hooks ---"
	@if [ ! -d "$(CURDIR)/.git" ]; then \
		echo "❌ .git directory not found. Initialize git first."; \
		exit 1; \
	fi
	@mkdir -p "$(CURDIR)/.git/hooks"
	@mkdir -p "$(CURDIR)/git-hooks"
	@ls -1A "$(CURDIR)/git-hooks" >/dev/null 2>&1 || true
	@count=0; \
	for name in $$(ls -1A "$(CURDIR)/git-hooks" 2>/dev/null); do \
		if [ -f "$(CURDIR)/git-hooks/$$name" ]; then \
			ln -sfn "../../git-hooks/$$name" "$(CURDIR)/.git/hooks/$$name"; \
			echo "    🔗 $$name -> ../../git-hooks/$$name"; \
			count=$$((count+1)); \
		fi; \
	done; \
	if [ $$count -eq 0 ]; then \
		echo "    ℹ️  No hook files found in git-hooks/."; \
	fi; \
	echo "--- Hooks linking complete ---"
