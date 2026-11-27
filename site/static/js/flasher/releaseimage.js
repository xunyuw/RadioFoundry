// SPDX-License-Identifier: MIT
// Copyright (c) 2026 The RadioFoundry Authors
// The site's own copy of a release image, when the selection asks for exactly the build that release holds.
//
// The site publishes each release's images beside its pages (firmware/<radio>/, copied from the GitHub release's assets
// by site/build.py --firmware, site/rfoundrysite/repo.py releases), so a visitor who picks the release's own feature set
// needs no build service: the image is a download. That matters most on a fork, whose page offers the fork's release but
// whose build requests would go to the project's service, which builds the project's tree and does not know the fork's
// switches (a fork's CJK_FONT_IN_FLASH came back "unknown feature"). Everything else still asks the service.
//
// The manifest's features= list is the build's switches (tools/release.py, make print-config), so the comparison is a
// set: the start language is left out, as it is everywhere a build is matched (update.js), and so are switches the
// release's radio can't have (they never appear in either list). A release with a build option (a radio's band, its
// power class) matches only the same value, and an option the selection names that the release doesn't carries no image.
// A version link is another ref, which no release here holds.
//
// No DOM here, so the host tests check it.

import { startCode } from "./langsuggest.js";

const switches = (list) => String(list || "").split(",").filter((n) => n && !startCode(n)).sort().join(",");

// The release entry ({version, features, build_options, image, manifest}) whose image is this selection exactly, or
// null. releases: this board's entries from releases.json, oldest first, so the newest match wins. board: the radio's
// id. options: the build values picked ({BAND: "vhf"}). pinned: a version link is in use, so the page builds that ref
// instead.
export function releaseImage(sel, { releases = [], board = null, options = {}, pinned = false } = {}) {
	if (pinned) return null;
	const rels = board ? releases.filter((r) => r.board === board) : releases;
	if (!rels.length) return null;
	const want = switches([...sel.on].join(","));
	// releases.json holds the releases oldest first (site/build.py release_list), so the last match is the newest: a
	// switch set that didn't change between two releases takes the later image, which is the one to install
	for (let i = rels.length - 1; i >= 0; i--) {
		const r = rels[i];
		if (switches(r.features) !== want) continue;
		// every option the selection names must be the one this image was built for; an option neither names is fine
		const have = r.build_options || {};
		if (Object.entries(options).some(([name, value]) => have[name] !== value)) continue;
		return r;
	}
	return null;
}
