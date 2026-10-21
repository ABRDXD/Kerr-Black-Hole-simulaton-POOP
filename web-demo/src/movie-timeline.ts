export interface MovieOptions {
  event: string;
  width: number;
  height: number;
  fps: number;
  duration: number;
  format: "mp4" | "png";
}

/** Output timestamps are i/fps. The full inspiral/plunge occupies 80% of the
 * sampled timeline; the remnant occupies 20%. This is an explicit cinematic
 * time remapping, not physical elapsed time or a change to the merger model. */
export function movieSample(index: number, options: Pick<MovieOptions, "fps" | "duration">,
  toMergerS: number, totalS: number): { clipSeconds: number; physicalSeconds: number } {
  const count = options.fps * options.duration;
  if (!Number.isInteger(count) || count < 2 || !Number.isInteger(index) || index < 0 || index >= count ||
      !Number.isFinite(toMergerS) || !Number.isFinite(totalS) || toMergerS <= 0 || totalS < toMergerS) {
    throw new Error("Invalid movie timeline.");
  }
  const fraction = index / (count - 1);
  const physicalSeconds = fraction <= 0.8
    ? toMergerS * fraction / 0.8
    : toMergerS + (totalS - toMergerS) * (fraction - 0.8) / 0.2;
  return { clipSeconds: index / options.fps, physicalSeconds };
}
