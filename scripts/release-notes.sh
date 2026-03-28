#!/bin/bash
# Generates categorized release notes from git history.
# Usage: ./scripts/release-notes.sh [current-tag] [previous-tag]

set -e

CURRENT_TAG=${1:-$GITHUB_REF_NAME}
PREVIOUS_TAG=${2:-$(git tag --sort=-version:refname | grep -v "^${CURRENT_TAG}$" | head -1)}

if [ -z "$CURRENT_TAG" ]; then
    echo "Error: No tag specified" >&2
    exit 1
fi

if [ -z "$PREVIOUS_TAG" ]; then
    echo "Warning: No previous tag found, using initial commit" >&2
    PREVIOUS_TAG=$(git rev-list --max-parents=0 HEAD)
fi

echo "Generating release notes: ${PREVIOUS_TAG}..${CURRENT_TAG}" >&2

{
    echo "## What's New in ${CURRENT_TAG}"
    echo ""

    FEATURES=$(git log --pretty=format:"- %s" "${PREVIOUS_TAG}..${CURRENT_TAG}" \
        --grep="feat:" --grep="add:" --grep="new:" --grep="feature:" -i)
    if [ -n "$FEATURES" ]; then
        echo "### Features"
        echo "$FEATURES"
        echo ""
    fi

    IMPROVEMENTS=$(git log --pretty=format:"- %s" "${PREVIOUS_TAG}..${CURRENT_TAG}" \
        --grep="improve:" --grep="enhance:" --grep="update:" --grep="refactor:" -i)
    if [ -n "$IMPROVEMENTS" ]; then
        echo "### Improvements"
        echo "$IMPROVEMENTS"
        echo ""
    fi

    FIXES=$(git log --pretty=format:"- %s" "${PREVIOUS_TAG}..${CURRENT_TAG}" \
        --grep="fix:" --grep="bug:" -i)
    if [ -n "$FIXES" ]; then
        echo "### Bug Fixes"
        echo "$FIXES"
        echo ""
    fi

    OTHER=$(git log --pretty=format:"- %s" "${PREVIOUS_TAG}..${CURRENT_TAG}" \
        --invert-grep \
        --grep="feat:" --grep="add:" --grep="new:" --grep="feature:" \
        --grep="improve:" --grep="enhance:" --grep="update:" --grep="refactor:" \
        --grep="fix:" --grep="bug:" -i)
    if [ -n "$OTHER" ]; then
        echo "### Other Changes"
        echo "$OTHER"
        echo ""
    fi

    echo "---"
    echo "**Full Changelog**: https://github.com/thorinside/nt_perform_vst/compare/${PREVIOUS_TAG}...${CURRENT_TAG}"
} > release_notes.md

cat release_notes.md
