import assert from "node:assert/strict";
import test from "node:test";
import { FrameTiming } from "../src/frame-timing.ts";

test("FPS is frames per elapsed time, including long foreground stalls", () => {
  const clock = new FrameTiming();
  assert.equal(clock.record(0, "kerr"), null);
  clock.record(10, "kerr");
  clock.record(100, "kerr");
  assert.equal(clock.snapshot().fps, 20); // Averaging 100 and 11.1 FPS would be wrong.
  clock.record(3100, "kerr");
  assert.equal(clock.snapshot().maxMs, 3000);
  assert.equal(clock.snapshot().durationMs, 3100);
  assert.equal(clock.snapshot().fps, 3000 / 3100);
});

test("capture reset excludes both the synchronous render and the following gap", () => {
  const clock = new FrameTiming();
  clock.record(0, "kerr");
  clock.record(20, "kerr");
  clock.reset(); // Capture is not itself a presented frame.
  assert.equal(clock.snapshot().fps, null);
  assert.equal(clock.record(5000, "kerr"), null);
  assert.equal(clock.record(5020, "kerr"), 20);
  assert.equal(clock.snapshot().fps, 50);
  assert.deepEqual(clock.samples(), [20]);
});

test("background and profile changes restart timing without stale results", () => {
  const clock = new FrameTiming();
  clock.record(0, "kerr");
  clock.record(20, "kerr");
  assert.equal(clock.record(40, "kerr", false), null);
  assert.equal(clock.record(10000, "kerr"), null);
  clock.record(10020, "kerr");
  assert.equal(clock.record(10040, "binary"), null);
  clock.record(10140, "binary");
  assert.deepEqual(clock.samples(), [100]);
});

test("RAF callbacks queued before capture completes cannot establish a stale baseline", () => {
  const clock = new FrameTiming();
  clock.record(0, "kerr");
  clock.record(20, "kerr");
  clock.reset(5000); // Capture and encoding completed at this timestamp.
  assert.equal(clock.record(4900, "kerr"), null);
  assert.equal(clock.record(4999, "different-size"), null);
  assert.equal(clock.record(5020, "different-size"), null);
  assert.equal(clock.record(5040, "different-size"), 20);
  assert.equal(clock.snapshot().fps, 50);
});

test("duplicate and stale timestamps cannot inflate FPS or reverse its clock", () => {
  const clock = new FrameTiming();
  clock.record(100, "kerr");
  assert.equal(clock.record(100, "kerr"), null);
  assert.equal(clock.record(90, "kerr"), null);
  assert.equal(clock.record(120, "kerr"), 20);
  assert.deepEqual(clock.samples(), [20]);
  for (const invalid of [NaN, Infinity, -Infinity]) {
    assert.equal(clock.record(invalid, "kerr"), null);
    assert.equal(clock.snapshot().sampleCount, 0);
    clock.record(200, "kerr");
  }
});

test("bounded history remains chronological after wrapping; percentiles use nearest rank", () => {
  const clock = new FrameTiming(4);
  let now = 0;
  clock.record(now, "kerr");
  for (const ms of [1, 2, 3, 4, 5, 6]) clock.record(now += ms, "kerr");
  assert.deepEqual(clock.samples(), [3, 4, 5, 6]);
  assert.deepEqual(clock.snapshot(), {
    sampleCount: 4, durationMs: 18, capacity: 4, fps: 4000 / 18,
    meanMs: 4.5, p50Ms: 4, p95Ms: 6, p99Ms: 6, maxMs: 6,
  });
  assert.equal(clock.snapshot(2).meanMs, 5.5);
  clock.samples().fill(1000);
  assert.deepEqual(clock.samples(), [3, 4, 5, 6]);
  clock.reset();
  clock.record(100, "kerr");
  clock.record(120, "kerr");
  assert.deepEqual(clock.samples(), [20]);
});

test("empty metrics serialize without invented rates or non-finite values", () => {
  const snapshot = new FrameTiming().snapshot();
  assert.equal(snapshot.fps, null);
  assert.equal(snapshot.p99Ms, null);
  assert.equal(snapshot.durationMs, 0);
  assert.deepEqual(JSON.parse(JSON.stringify(snapshot)), snapshot);
  for (const size of [0, -1, 1.5, NaN, Infinity, 36_001]) {
    assert.throws(() => new FrameTiming(size), RangeError);
  }
});
