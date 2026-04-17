import { writable } from 'svelte/store';

export interface GpsOrigin {
  lat: number;
  lon: number;
}

/** Set when the active map uses WGS84 GPS coordinates (App-recorded maps).
 *  Used by MapCanvas to render OSM tile background and align the robot overlay. */
export const mapGpsOrigin = writable<GpsOrigin | null>(null);
