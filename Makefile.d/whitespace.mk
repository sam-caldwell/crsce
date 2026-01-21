.PHONY: whitespace/fix

# Normalize line endings to LF, trim trailing whitespace, and ensure a single
# newline at EOF for all tracked text files. Uses git to select only text files.
whitespace/fix:
	@bash -lc 'set -euo pipefail; \
	  files="$$(git grep -Il -- . || true)"; \
	  if [ -z "$$files" ]; then echo "whitespace-fix: no tracked text files found"; exit 0; fi; \
	  echo "whitespace-fix: normalizing files..."; \
	  for f in $$files; do \
	    tmp="$$(mktemp)"; tmp2="$$(mktemp)"; \
	    LC_ALL=C sed -e "s/\r//g" -e "s/[[:space:]]\+$$//" "$$f" > "$$tmp"; \
	    perl -0777 -pe "s/\n*\z/\n/" "$$tmp" > "$$tmp2"; \
	    if ! cmp -s "$$tmp2" "$$f"; then mv "$$tmp2" "$$f"; fi; \
	    rm -f "$$tmp" "$$tmp2"; \
	    git add $f; \
	  done; \
	  echo "whitespace-fix: done"'
