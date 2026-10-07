/** Visible animation-frame intervals, independent of simulation and captures. */
export class FrameTiming {
  private previous: number | null = null;
  private profile = "";
  private intervals: number[] = [];
  private cursor = 0;
  private capacity: number;
  private notBefore = -Infinity;

  constructor(capacity = 3600) {
    if (!Number.isInteger(capacity) || capacity < 1 || capacity > 36_000) {
      throw new RangeError("Frame timing capacity must be between 1 and 36000.");
    }
    this.capacity = capacity;
  }

  /** Returns a complete interval, or null when establishing a new baseline. */
  record(now: number, profile: string, visible = true): number | null {
    if (!visible || !Number.isFinite(now)) {
      this.reset();
      return null;
    }
    if (now < this.notBefore) return null;
    if (profile !== this.profile) this.reset();
    this.profile = profile;
    if (this.previous === null) {
      this.previous = now;
      return null;
    }
    // A queued RAF may predate a synchronous capture or have a duplicate stamp.
    if (now <= this.previous) return null;
    const elapsed = now - this.previous;
    this.previous = now;
    if (!Number.isFinite(elapsed)) {
      this.reset();
      return null;
    }
    if (this.intervals.length < this.capacity) this.intervals.push(elapsed);
    else this.intervals[this.cursor] = elapsed;
    this.cursor = (this.cursor + 1) % this.capacity;
    return elapsed;
  }

  /** Discard capture, background and configuration-change gaps. */
  reset(notBefore = -Infinity): void {
    this.previous = null;
    this.intervals = [];
    this.cursor = 0;
    this.notBefore = Number.isFinite(notBefore) ? notBefore : -Infinity;
  }

  /** Chronological copy; never exposes the mutable bounded history. */
  samples(): number[] {
    if (this.intervals.length < this.capacity) return [...this.intervals];
    return [...this.intervals.slice(this.cursor), ...this.intervals.slice(0, this.cursor)];
  }

  snapshot(recent = this.capacity) {
    if (!Number.isInteger(recent) || recent < 1 || recent > this.capacity) {
      throw new RangeError("Invalid frame timing window.");
    }
    const samples = this.samples().slice(-recent);
    const count = samples.length;
    const durationMs = samples.reduce((sum, ms) => sum + ms, 0);
    const sorted = samples.sort((a, b) => a - b);
    // Nearest-rank percentiles. Real foreground stalls are retained, uncapped.
    const percentile = (p: number): number | null =>
      count ? sorted[Math.ceil(count * p) - 1]! : null;
    return {
      sampleCount: count, durationMs, capacity: this.capacity,
      fps: count ? 1000 * count / durationMs : null,
      meanMs: count ? durationMs / count : null,
      p50Ms: percentile(0.5), p95Ms: percentile(0.95), p99Ms: percentile(0.99),
      maxMs: count ? sorted[count - 1]! : null,
    };
  }
}
