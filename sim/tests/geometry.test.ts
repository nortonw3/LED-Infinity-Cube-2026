import { describe, it, expect } from 'vitest';
import { NUM_LEDS, NUM_EDGES, LEDS_PER_EDGE, voxels, graphL, graphR } from '../src/geometry';

describe('geometry', () => {
  it('has 384 voxels on 12 edges of 32', () => {
    expect(NUM_EDGES).toBe(12);
    expect(LEDS_PER_EDGE).toBe(32);
    expect(NUM_LEDS).toBe(384);
    expect(voxels.length).toBe(384);
  });

  it('edge 0 runs from A(0,0,0) to B(1,0,0)', () => {
    expect(voxels[0]).toEqual({ x: 0, y: 0, z: 0 });
    expect(voxels[31]).toEqual({ x: 1, y: 0, z: 0 });
    expect(voxels[16].x).toBeCloseTo(16 / 31, 6);
    expect(voxels[16].y).toBe(0);
  });

  it('every voxel coordinate is within [0,1]', () => {
    for (const v of voxels) {
      expect(v.x).toBeGreaterThanOrEqual(0); expect(v.x).toBeLessThanOrEqual(1);
      expect(v.y).toBeGreaterThanOrEqual(0); expect(v.y).toBeLessThanOrEqual(1);
      expect(v.z).toBeGreaterThanOrEqual(0); expect(v.z).toBeLessThanOrEqual(1);
    }
  });

  it('interior LEDs chain along their own edge', () => {
    expect(graphL[5]).toBe(4);
    expect(graphR[5]).toBe(6);
  });

  it('vertex welds connect different edges (endpoint neighbor is NOT on own edge)', () => {
    // LED 0 is edge 0's base at vertex A; after welding, its L-neighbor must be
    // an endpoint of another edge meeting at A — not LED 31 of its own edge.
    const ownEdge = new Set(Array.from({ length: 32 }, (_, i) => i));
    expect(ownEdge.has(graphL[0])).toBe(false);
  });

  it('every LED has valid, non-self neighbors', () => {
    for (let i = 0; i < NUM_LEDS; i++) {
      expect(graphL[i]).toBeGreaterThanOrEqual(0); expect(graphL[i]).toBeLessThan(NUM_LEDS);
      expect(graphR[i]).toBeGreaterThanOrEqual(0); expect(graphR[i]).toBeLessThan(NUM_LEDS);
      expect(graphL[i]).not.toBe(i);
      expect(graphR[i]).not.toBe(i);
    }
  });
});
