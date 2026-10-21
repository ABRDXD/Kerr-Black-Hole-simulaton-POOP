import assert from "node:assert/strict";
import test from "node:test";
import { movieSample } from "../src/movie-timeline.ts";

for (const fps of [24, 30, 60, 144]) {
  test(`12-second ${fps} FPS timeline has distinct ordered times and includes both merger endpoints`, () => {
    const options = { duration: 12, fps };
    const frames = Array.from({ length: 12 * fps }, (_, index) => movieSample(index, options, 0.4, 0.46));
    assert.equal(frames.length, fps * 12);
    assert.deepEqual(frames[0], { clipSeconds: 0, physicalSeconds: 0 });
    assert(Math.abs(frames.at(-1)!.physicalSeconds - 0.46) < 1e-14);
    assert.equal(frames.at(-1)!.clipSeconds + 1 / fps, 12);
    for (let index = 1; index < frames.length; index++) {
      assert(frames[index]!.physicalSeconds > frames[index - 1]!.physicalSeconds);
      assert(Math.abs(frames[index]!.clipSeconds - frames[index - 1]!.clipSeconds - 1 / fps) < 1e-14);
    }
    const collision = frames.findIndex(frame => frame.physicalSeconds >= 0.4);
    assert(Math.abs(collision / (frames.length - 1) - 0.8) < 1 / (frames.length - 1));
    // Sampling again in an arbitrary order gives the same state: wall time is absent.
    for (const index of [frames.length - 1, 0, 11, 20]) assert.deepEqual(movieSample(index, options, 0.4, 0.46), frames[index]);
    assert.throws(() => movieSample(frames.length, options, 0.4, 0.46));
    assert.throws(() => movieSample(-1, options, 0.4, 0.46));
  });
}

test('invalid physical timelines fail explicitly', () => {
  for (const [merger, total] of [[0, 1], [1, 0], [NaN, 1], [1, Infinity]]) {
    assert.throws(() => movieSample(0, { fps: 60, duration: 12 }, merger, total));
  }
});
