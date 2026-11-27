// SPDX-License-Identifier: MIT
// Copyright (c) 2026 The RadioFoundry Authors
// The site's own release image (flasher/releaseimage.js): when a selection is exactly a release's build, the page
// installs that image instead of asking the build service. A fork's page depends on this: the service builds the
// project's tree and answers a fork-only switch with "unknown feature".

import { test } from "node:test";
import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import { releaseImage } from "../static/js/flasher/releaseimage.js";

const sel = (...on) => ({ on: new Set(on) });
const rel = (features, extra = {}) => ({ board: "mduv380", version: "v26.11", features, image: "a.bin", manifest: "a.json", ...extra });

test("the release's own switch set is its image; the start language and the order don't matter", () => {
	const r = rel("FM,DMR,YSF,LANG_ZH,LANG_DEFAULT_ZH");
	const got = releaseImage(sel("DMR", "FM", "YSF", "LANG_ZH", "LANG_DEFAULT_ZH"), { releases: [r], board: "mduv380" });
	assert.equal(got, r);
	// a switch the selection has that the release's build left on is the same build: the list is a set
	assert.equal(releaseImage(sel("YSF", "LANG_ZH", "DMR", "FM", "LANG_DEFAULT_ZH"), { releases: [r], board: "mduv380" }), r);
});

test("a selection that differs in one switch has no release image: that is a custom build", () => {
	const r = rel("FM,DMR,YSF");
	// the CJK switches are the ones a fork's release carries and the service does not know
	assert.equal(releaseImage(sel("FM", "DMR", "YSF", "CJK_FONT_IN_FLASH"), { releases: [r], board: "mduv380" }), null);
	assert.equal(releaseImage(sel("FM", "DMR"), { releases: [r], board: "mduv380" }), null);
	assert.equal(releaseImage(sel("FM", "DMR", "YSF", "PINYIN"), { releases: [r], board: "mduv380" }), null);
});

test("another radio's release, a version link and no release at all give none", () => {
	const phone = rel("FM,DMR", { board: "dm1701" });
	assert.equal(releaseImage(sel("FM", "DMR"), { releases: [phone], board: "mduv380" }), null);
	assert.equal(releaseImage(sel("FM", "DMR"), { releases: [rel("FM,DMR")], board: "mduv380", pinned: true }), null);
	assert.equal(releaseImage(sel("FM"), { releases: [], board: "mduv380" }), null);
	// no board named: every entry counts (the caller's own list is this board's)
	assert.equal(releaseImage(sel("FM", "DMR"), { releases: [phone] }), phone);
});

test("a build option must be the image's: a radio's band or power class", () => {
	const uhf = rel("FM,DMR", { build_options: { POWER: "5w" } });
	assert.equal(releaseImage(sel("FM", "DMR"), { releases: [uhf], board: "mduv380", options: { POWER: "5w" } }), uhf);
	assert.equal(releaseImage(sel("FM", "DMR"), { releases: [uhf], board: "mduv380", options: { POWER: "10w" } }), null);
	// an image built for the default (no value recorded) is no match for a radio that names one
	assert.equal(releaseImage(sel("FM", "DMR"), { releases: [rel("FM,DMR")], board: "mduv380", options: { POWER: "5w" } }), null);
	// no option named by the selection: the plain image is it
	assert.deepEqual(releaseImage(sel("FM", "DMR"), { releases: [rel("FM,DMR")], board: "mduv380" }), rel("FM,DMR"));
});

test("the newest release wins when two hold the same build", () => {
	const old = rel("FM,DMR", { version: "v26.10" });
	const now = rel("FM,DMR", { version: "v26.11" });
	// releases.json is oldest first (site/build.py release_list), so the last one whose build matches is the newest
	assert.equal(releaseImage(sel("FM", "DMR"), { releases: [old, now], board: "mduv380" }), now);
});

test("the flasher asks the site's own image before the build service", () => {
	const src = readFileSync(new URL("../static/js/flasher/app.js", import.meta.url), "utf8");
	// the fetch-and-check path is the service's, on a site-relative URL, and it comes before the POST
	const at = src.indexOf("const owned = releaseImage(sel");
	assert.ok(at > 0, "requestBuild asks releaseImage");
	const post = src.indexOf('api("POST", "builds"', at);
	assert.ok(post > at, "the POST comes after the site's image");
	assert.match(src.slice(at, post), /fetch\(ROOT \+ owned\.manifest\)/);
	assert.match(src.slice(at, post), /await verify\(m, img, fb\(\), S\.board\.id\)/);
	// every language says what the two paths do
	for (const lang of ["en", "es", "it", "zh"]) {
		const cat = JSON.parse(readFileSync(new URL(`../i18n/${lang}.json`, import.meta.url), "utf8"));
		for (const k of ["flash.build.fetching", "flash.build.release_image", "flash.build.from_release"]) assert.ok(cat[k], `${lang}: ${k}`);
		assert.match(cat["flash.build.from_release"], /\{version\}/, lang);
	}
});
