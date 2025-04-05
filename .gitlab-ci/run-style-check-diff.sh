#!/bin/bash

set -e

ancestor_horizon=31  # days (one month)

# Recently, git is picky about directory ownership. Tell it not to worry.
git config --global --add safe.directory "$PWD"

# We need to add a new remote for the upstream target branch, since this script
# could be running in a personal fork of the repository which has out of date
# branches.
#
# Limit the fetch to a certain date horizon to limit the amount of data we get.
# If the branch was forked from origin/main before this horizon, it should
# probably be rebased.
if ! git ls-remote --exit-code upstream >/dev/null 2>&1 ; then
    git remote add upstream https://gitlab.gnome.org/GNOME/gnome-calendar.git
fi

# Work out the newest common ancestor between the detached HEAD that this CI job
# has checked out, and the upstream target branch (which will typically be
# `upstream/main` or `upstream/glib-2-62`).
# `${CI_MERGE_REQUEST_TARGET_BRANCH_NAME}` or `${CI_MERGE_REQUEST_SOURCE_BRANCH_NAME}`
# are only defined if we’re running in a merge request pipeline,
# fall back to `${CI_DEFAULT_BRANCH}` or `${CI_COMMIT_BRANCH}` respectively
# otherwise.

source_branch="${CI_MERGE_REQUEST_SOURCE_BRANCH_NAME:-${CI_COMMIT_BRANCH}}"
target_branch="${CI_MERGE_REQUEST_TARGET_BRANCH_NAME:-${CI_DEFAULT_BRANCH}}"
git fetch --shallow-since="$(date --date="${ancestor_horizon} days ago" +%Y-%m-%d)" origin "${source_branch}"
git fetch --shallow-since="$(date --date="${ancestor_horizon} days ago" +%Y-%m-%d)" upstream "${target_branch}"

newest_common_ancestor_sha=$(git merge-base upstream/${target_branch} origin/${source_branch})
if [ -z "${newest_common_ancestor_sha}" ]; then
    echo "Couldn’t find common ancestor with upstream main branch. This typically"
    echo "happens if you branched from main a long time ago. Please update"
    echo "your clone, rebase, and re-push your branch."
    exit 1
fi

git diff -U0 --no-color "${newest_common_ancestor_sha}" | .gitlab-ci/clang-format-diff.py -binary "clang-format" -p1
exit_status=$?

# The style check is not infallible. The clang-format configuration cannot
# perfectly describe GTK’s coding style: in particular, it cannot align
# function arguments. The documented coding style for GTK takes priority over
# clang-format suggestions. Hopefully we can eventually improve clang-format to
# be configurable enough for our coding style. That’s why this CI check is OK
# to fail: the idea is that people can look through the output and ignore it if
# it’s wrong. (That situation can also happen if someone touches pre-existing
# badly formatted code and it doesn’t make sense to tidy up the wider coding
# style with the changes they’re making.)
echo ""
echo "Note that clang-format output is advisory and cannot always match the"
echo "GTK coding style, documented at:"
echo "   https://gitlab.gnome.org/GNOME/gtk/blob/main/docs/CODING-STYLE.md"
echo "Warnings from this tool can be ignored in favour of the documented "
echo "coding style, or in favour of matching the style of existing"
echo "surrounding code."

exit ${exit_status}

