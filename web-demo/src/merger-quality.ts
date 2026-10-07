/** Presentation-only ray-count control. It never changes integration settings. */
export interface MergerQualityConfig {
  /** Canvas dimensions in device pixels, before the user's resolution scale. */
  readonly width: number;
  readonly height: number;
  readonly requestedScale: number;
  readonly active: boolean;
  readonly retarded: boolean;
  readonly enabled: boolean;
}

export interface MergerQualitySize {
  readonly width: number;
  readonly height: number;
  readonly requestedWidth: number;
  readonly requestedHeight: number;
  readonly automatic: boolean;
  readonly atFloor: boolean;
}

const FROZEN = { initial: 192_000, maximum: 300_000 } as const;
const RETARDED = { initial: 96_000, maximum: 192_000 } as const;
const MINIMUM_PIXELS = 48_000;
const TARGET_MS = 1000 / 30;
const WINDOW_MS = 400;
const HEADROOM_MS = 2400;
const MAX_SAMPLE_MS = 2000;
const SEVERE_FRAME_MS = 100;

/**
 * Bound the first binary frame immediately, then adjust only its pixel count.
 * sample() should receive foreground RAF intervals, not CPU submission times
 * or synchronous capture/benchmark renders. Reset timing across those renders
 * and visibility changes. A profile change restores that profile's initial
 * budget; the user's requested dimensions remain an upper bound throughout.
 */
export class MergerQuality {
  private key = "";
  private automatic = false;
  private pixelBudget: number = FROZEN.initial;
  private minimum = MINIMUM_PIXELS;
  private maximum: number = FROZEN.maximum;
  private windowMs = 0;
  private windowSamples = 0;
  private headroomMs = 0;
  private pendingLongFrame = false;

  configure(config: MergerQualityConfig): MergerQualitySize {
    const deviceWidth = this.deviceDimension(config.width);
    const deviceHeight = this.deviceDimension(config.height);
    const scale = Number.isFinite(config.requestedScale) && config.requestedScale > 0
      ? config.requestedScale : 1;
    const width = Math.max(8, Math.round(deviceWidth * scale));
    const height = Math.max(8, Math.round(deviceHeight * scale));
    const automatic = config.active && config.enabled;
    const key = `${deviceWidth}:${deviceHeight}:${scale}:${config.active}:${config.enabled}:${config.retarded}`;
    if (key !== this.key) {
      this.key = key;
      this.automatic = automatic;
      const requestedPixels = width * height;
      const profile = config.retarded ? RETARDED : FROZEN;
      this.minimum = Math.min(MINIMUM_PIXELS, requestedPixels);
      this.maximum = Math.min(profile.maximum, requestedPixels);
      this.pixelBudget = Math.min(profile.initial, requestedPixels);
      this.resetTiming();
    }
    if (!automatic) {
      return {
        width, height, requestedWidth: width, requestedHeight: height,
        automatic: false, atFloor: false,
      };
    }
    const factor = Math.min(1, Math.sqrt(this.pixelBudget / (width * height)));
    return {
      // Truncate both dimensions with the same factor. No upscaling, and the
      // remaining aspect error is limited to less than one pixel per axis.
      width: Math.max(1, Math.floor(width * factor)),
      height: Math.max(1, Math.floor(height * factor)),
      requestedWidth: width,
      requestedHeight: height,
      automatic: true,
      atFloor: this.pixelBudget <= this.minimum,
    };
  }

  sample(frameMs: number): void {
    if (!this.automatic || !Number.isFinite(frameMs) || frameMs <= 0) return;
    // One long interval may be a pause. Hold it aside until another slow
    // foreground frame confirms overload; otherwise sub-0.5-fps rendering
    // would be mistaken for repeated pauses and never reduce its workload.
    if (frameMs > MAX_SAMPLE_MS && !this.pendingLongFrame) {
      this.resetTiming();
      this.pendingLongFrame = true;
      return;
    }
    if (this.pendingLongFrame) {
      this.pendingLongFrame = false;
      if (frameMs >= SEVERE_FRAME_MS) {
        this.windowMs = MAX_SAMPLE_MS;
        this.windowSamples = 1;
      }
    }
    // Confirmed long frames only need bounded evidence, not their entire
    // duration. Existing cooldowns, reduction limits and floor still apply.
    frameMs = Math.min(frameMs, MAX_SAMPLE_MS);
    this.windowMs += frameMs;
    this.windowSamples++;
    const meanMs = this.windowMs / this.windowSamples;
    const minimumSamples = meanMs >= SEVERE_FRAME_MS ? 2 : 4;
    if (this.windowMs < WINDOW_MS || this.windowSamples < minimumSamples) return;

    if (meanMs > TARGET_MS * 1.25) {
      // Pixel cost is approximately linear. Halve at most per decision so a
      // single delayed frame cannot force the render straight to its floor.
      const ratio = Math.max(0.5, Math.min(0.85, TARGET_MS / meanMs));
      this.pixelBudget = Math.max(this.minimum, this.pixelBudget * ratio);
      this.headroomMs = 0;
    } else if (meanMs < TARGET_MS * 0.75) {
      this.headroomMs += this.windowMs;
      if (this.headroomMs >= HEADROOM_MS) {
        this.pixelBudget = Math.min(this.maximum, this.pixelBudget * 1.12);
        this.headroomMs = 0;
      }
    } else {
      this.headroomMs = 0;
    }
    this.windowMs = 0;
    this.windowSamples = 0;
  }

  /** Clear history, retaining the current pixel budget and requested size. */
  resetTiming(): void {
    this.windowMs = 0;
    this.windowSamples = 0;
    this.headroomMs = 0;
    this.pendingLongFrame = false;
  }

  private deviceDimension(value: number): number {
    return Number.isFinite(value) ? Math.max(1, Math.round(value)) : 1;
  }
}
