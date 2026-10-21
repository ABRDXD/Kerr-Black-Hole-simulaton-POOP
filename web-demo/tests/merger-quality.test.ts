import assert from "node:assert/strict";
import test from "node:test";
import { MergerQuality, type MergerQualityConfig } from "../src/merger-quality.ts";

const base: MergerQualityConfig = Object.freeze({
  width: 1920, height: 1080, requestedScale: 1,
  active: true, retarded: false, enabled: true,
});
const pixels = (size: { width: number; height: number }): number => size.width * size.height;
const feed = (quality: MergerQuality, count: number, frameMs: number): void => {
  for (let i = 0; i < count; i++) quality.sample(frameMs);
};

test("first binary frame is bounded at full HD and DPR2 physical dimensions", () => {
  for (const viewport of [base, { ...base, width: 3840, height: 2160 }]) {
    const quality = new MergerQuality();
    const size = quality.configure(viewport);
    assert.equal(size.automatic, true);
    assert.equal(size.atFloor, false);
    assert(pixels(size) <= 192_000);
    assert(pixels(size) > 190_000);
    assert.equal(size.requestedWidth, viewport.width);
    assert.equal(size.requestedHeight, viewport.height);
    const retarded = quality.configure({ ...viewport, retarded: true });
    assert(pixels(retarded) <= 96_000);
    assert(pixels(retarded) > 94_000);
  }
});

test("manual, disabled, and single-hole rendering retain exact requested dimensions", () => {
  for (const config of [
    { ...base, enabled: false },
    { ...base, active: false },
    { ...base, width: 1235, height: 987, requestedScale: 0.4, enabled: false },
  ]) {
    const quality = new MergerQuality();
    const size = quality.configure(config);
    assert.equal(size.width, Math.round(config.width * config.requestedScale));
    assert.equal(size.height, Math.round(config.height * config.requestedScale));
    assert.equal(size.automatic, false);
    assert.equal(size.atFloor, false);
    feed(quality, 40, 1000);
    assert.deepEqual(quality.configure(config), size);
  }
});

test("severe overload waits for evidence, then drops rapidly with a bounded floor", () => {
  const quality = new MergerQuality();
  const initial = quality.configure(base);
  quality.sample(500);
  assert.deepEqual(quality.configure(base), initial, "one spike is insufficient");
  quality.sample(500);
  const reduced = quality.configure(base);
  assert(pixels(reduced) < pixels(initial) * 0.51);
  assert(pixels(reduced) > 94_000);
  feed(quality, 40, 500);
  const floor = quality.configure(base);
  assert.equal(floor.atFloor, true);
  assert(pixels(floor) <= 48_000);
  assert(pixels(floor) > 47_000);
  feed(quality, 40, 1000);
  assert.deepEqual(quality.configure(base), floor);
});

test("sustained foreground frames longer than two seconds downshift to the floor", () => {
  for (const retarded of [false, true]) {
    for (const frameMs of [2001, 5000, Number.MAX_VALUE]) {
      const quality = new MergerQuality();
      const config = { ...base, retarded };
      const initial = quality.configure(config);
      quality.sample(frameMs);
      assert.deepEqual(quality.configure(config), initial, "one long pause is ignored");
      quality.sample(frameMs);
      const reduced = quality.configure(config);
      assert(pixels(reduced) < pixels(initial), "a second slow foreground frame confirms overload");
      feed(quality, 10, frameMs);
      const floor = quality.configure(config);
      assert.equal(floor.atFloor, true);
      assert(pixels(floor) > 47_000 && pixels(floor) <= 48_000);
      feed(quality, 120, 20);
      assert(pixels(quality.configure(config)) > pixels(floor), "quality can recover after overload ends");
    }
  }
});

test("long frames alternating with shorter severe frames still establish sustained overload", () => {
  const quality = new MergerQuality();
  const initial = quality.configure(base);
  quality.sample(3000);
  quality.sample(1000);
  assert(pixels(quality.configure(base)) < pixels(initial));
  quality.sample(3000);
  quality.sample(1000);
  assert.equal(quality.configure(base).atFloor, true);
});

test("isolated long pauses followed by normal frames never accumulate overload evidence", () => {
  const quality = new MergerQuality();
  const initial = quality.configure(base);
  for (let i = 0; i < 5; i++) {
    quality.sample(10_000);
    quality.sample(16);
    assert.deepEqual(quality.configure(base), initial);
  }
});

test("timing and profile resets discard pending long-frame evidence", () => {
  const quality = new MergerQuality();
  const initial = quality.configure(base);
  quality.sample(3000);
  quality.resetTiming();
  quality.sample(3000);
  assert.deepEqual(quality.configure(base), initial, "a known visibility gap starts a fresh history");
  const config = { ...base, retarded: true };
  const retardedInitial = quality.configure(config);
  quality.sample(3000);
  assert.deepEqual(quality.configure(config), retardedInitial, "old mode evidence cannot affect the new profile");
});

test("ordinary overload observes sample and time cooldowns", () => {
  const quality = new MergerQuality();
  const initial = quality.configure(base);
  feed(quality, 6, 60);
  assert.deepEqual(quality.configure(base), initial, "360ms is below decision interval");
  quality.sample(60);
  const reduced = quality.configure(base);
  assert(pixels(reduced) < pixels(initial));
  feed(quality, 6, 60);
  assert.deepEqual(quality.configure(base), reduced, "new interval starts after adjustment");
});

test("quality recovers slowly after sustained headroom and respects profile ceilings", () => {
  for (const retarded of [false, true]) {
    const config = { ...base, retarded };
    const quality = new MergerQuality();
    quality.configure(config);
    feed(quality, 8, 500);
    const floor = quality.configure(config);
    feed(quality, 100, 20);
    assert.deepEqual(quality.configure(config), floor, "2s does not establish recovery");
    feed(quality, 20, 20);
    const recovering = quality.configure(config);
    assert(pixels(recovering) > pixels(floor));
    assert(pixels(recovering) < pixels(floor) * 1.14, "one recovery step is gentle");
    feed(quality, 6000, 20);
    const ceiling = quality.configure(config);
    const maximum = retarded ? 192_000 : 300_000;
    assert(pixels(ceiling) <= maximum);
    assert(pixels(ceiling) > maximum - 2000);
  }
});

test("hysteresis prevents resizing at acceptable frame times", () => {
  const quality = new MergerQuality();
  const initial = quality.configure(base);
  for (let i = 0; i < 120; i++) {
    quality.sample(i % 2 ? 30 : 39);
  }
  assert.deepEqual(quality.configure(base), initial);
});

test("invalid, stale, and hidden-gap measurements cannot poison the controller", () => {
  const quality = new MergerQuality();
  const initial = quality.configure(base);
  for (const frameMs of [NaN, Infinity, -Infinity, 0, -1, -500]) quality.sample(frameMs);
  assert.deepEqual(quality.configure(base), initial);
  feed(quality, 100, 20);
  quality.sample(10_000);
  feed(quality, 20, 20);
  assert.deepEqual(quality.configure(base), initial, "suspend gap clears previous headroom");
});

test("resetTiming discards capture and visibility timing history without changing quality", () => {
  const quality = new MergerQuality();
  quality.configure(base);
  feed(quality, 4, 500);
  const floor = quality.configure(base);
  feed(quality, 100, 20);
  quality.resetTiming();
  assert.deepEqual(quality.configure(base), floor);
  feed(quality, 20, 20);
  assert.deepEqual(quality.configure(base), floor);
});

test("profile, viewport, and manual scale changes reset accumulated decisions", () => {
  const quality = new MergerQuality();
  quality.configure(base);
  feed(quality, 4, 500);
  assert.equal(quality.configure(base).atFloor, true);
  const retarded = quality.configure({ ...base, retarded: true });
  assert(pixels(retarded) > 94_000);
  const resized = quality.configure({ ...base, width: 1280, height: 720 });
  assert(pixels(resized) > 190_000);
  feed(quality, 100, 20);
  const scaled = quality.configure({ ...base, requestedScale: 0.5 });
  feed(quality, 20, 20);
  assert.deepEqual(quality.configure({ ...base, requestedScale: 0.5 }), scaled);
  quality.configure({ ...base, active: false });
  assert(pixels(quality.configure(base)) > 190_000);
});

test("automatic sizing preserves aspect within pixel rounding and never exceeds requested size", () => {
  for (const [width, height, requestedScale] of [
    [1920, 1080, 1], [1080, 1920, 0.75], [3440, 1440, 0.4],
    [3839, 2159, 1], [100, 100, 0.5], [16_000, 8, 1],
  ]) {
    const config = { ...base, width: width!, height: height!, requestedScale: requestedScale!, retarded: true };
    const quality = new MergerQuality();
    quality.configure(config);
    for (const frameMs of [500, 20]) {
      feed(quality, 3000, frameMs);
      const size = quality.configure(config);
      assert(size.width >= 1 && size.height >= 1);
      assert(size.width <= size.requestedWidth && size.height <= size.requestedHeight);
      assert(pixels(size) <= 192_000);
      const aspectError = Math.abs(size.width / size.requestedWidth - size.height / size.requestedHeight);
      assert(aspectError <= Math.max(1 / size.requestedWidth, 1 / size.requestedHeight) + 1e-12);
    }
  }
});

test("small manual ceilings never upscale and input settings are never mutated", () => {
  const config = Object.freeze({ ...base, width: 400, height: 240, requestedScale: 0.5, maxSteps: 400 });
  const quality = new MergerQuality();
  const initial = quality.configure(config);
  assert.deepEqual(initial, {
    width: 200, height: 120, requestedWidth: 200, requestedHeight: 120,
    automatic: true, atFloor: true,
  });
  feed(quality, 400, 500);
  feed(quality, 6000, 20);
  assert.deepEqual(quality.configure(config), initial);
  assert.equal(config.maxSteps, 400);
  assert.equal(config.requestedScale, 0.5);
  assert.equal(config.active, true);
});
