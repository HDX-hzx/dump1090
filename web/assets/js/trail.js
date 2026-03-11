// Adaptive sampling constants (used by main.js via import).
export const HEADING_CHANGE_DEG = 2;
export const MAX_DISTANCE_NM = 1.5;
export const MIN_DISTANCE_NM = 0.03;
export const ALT_CHANGE_FT = 1000;

// Color stops: altitude (ft) -> HSL.
const COLOR_STOPS = [
    { alt: 0,     h: 0,   s: 0,  l: 95 },  // white
    { alt: 1000,  h: 60,  s: 95, l: 55 },  // yellow
    { alt: 2500,  h: 120, s: 80, l: 40 },  // green
    { alt: 5000,  h: 180, s: 85, l: 45 },  // cyan
    { alt: 10000, h: 210, s: 80, l: 60 },  // light blue
    { alt: 20000, h: 220, s: 75, l: 45 },  // blue
    { alt: 30000, h: 270, s: 60, l: 40 },  // purple
    { alt: 45000, h: 280, s: 70, l: 25 },  // dark purple
];

/**
 * Returns a color for a given altitude using piecewise linear interpolation
 * across the COLOR_STOPS table.
 *
 * @param {number | null | undefined} altitude - altitude in feet
 * @returns {string}
 */
export function altitudeColor(altitude) {
    const alt = Math.max(0, Math.min(45000, altitude ?? 0));

    // Find the surrounding stops.
    let lo = COLOR_STOPS[0];
    let hi = COLOR_STOPS[COLOR_STOPS.length - 1];
    for (let i = 0; i < COLOR_STOPS.length - 1; i++) {
        if (alt >= COLOR_STOPS[i].alt && alt <= COLOR_STOPS[i + 1].alt) {
            lo = COLOR_STOPS[i];
            hi = COLOR_STOPS[i + 1];
            break;
        }
    }

    const t = hi.alt === lo.alt ? 0 : (alt - lo.alt) / (hi.alt - lo.alt);
    const h = lo.h + t * (hi.h - lo.h);
    const s = lo.s + t * (hi.s - lo.s);
    const l = lo.l + t * (hi.l - lo.l);
    return `hsl(${h}, ${s}%, ${l}%)`;
}

/**
 * Returns the approximate distance in nautical miles between two lat/lon points
 * using the equirectangular approximation.
 */
export function distanceNM(lat1, lon1, lat2, lon2) {
    const R_NM = 3440.065;
    const dLat = (lat2 - lat1) * Math.PI / 180;
    const dLon = (lon2 - lon1) * Math.PI / 180;
    const cosLat = Math.cos((lat1 + lat2) / 2 * Math.PI / 180);
    return R_NM * Math.sqrt(dLat * dLat + dLon * dLon * cosLat * cosLat);
}

/**
 * Returns the absolute angular difference in degrees between two headings,
 * correctly handling the 360/0 wrap. Returns 360 if either value is null
 * so that a point is always recorded when heading is unknown.
 *
 * @param {number | null | undefined} a
 * @param {number | null | undefined} b
 * @returns {number}
 */
export function angleDifference(a, b) {
    if (a == null || b == null) return 360;
    const d = Math.abs(a - b) % 360;
    return d > 180 ? 360 - d : d;
}

/** @type {L.LayerGroup | null} */
let layer = null;

/**
 * Draws the trail for an aircraft on the given map.
 * Every trail point gets a dot; lines connect them.
 *
 * @param {L.Map} map
 * @param {{lat: number, lon: number, altitude: number | null, trail: Array<{lat: number, lon: number, altitude: number | null}>}} aircraft
 * @returns {void}
 */
export function drawTrail(map, aircraft) {
    removeTrail(map);
    if (!aircraft?.trail || aircraft.trail.length < 1) return;

    layer = L.layerGroup().addTo(map);

    const trail = aircraft.trail;

    // Draw connecting line segments.
    for (let i = 1; i < trail.length; i++) {
        const color = altitudeColor(trail[i].altitude);
        L.polyline(
            [[trail[i - 1].lat, trail[i - 1].lon], [trail[i].lat, trail[i].lon]],
            { color, weight: 2, opacity: 0.6, lineCap: 'butt', lineJoin: 'round' }
        ).addTo(layer);
    }

    // Connect the last trail point to the aircraft's current position.
    const last = trail[trail.length - 1];
    if (aircraft.lat !== last.lat || aircraft.lon !== last.lon) {
        const color = altitudeColor(aircraft.altitude);
        L.polyline(
            [[last.lat, last.lon], [aircraft.lat, aircraft.lon]],
            { color, weight: 2, opacity: 0.6, lineCap: 'butt', lineJoin: 'round' }
        ).addTo(layer);
    }

    // Draw dots on top of lines at every trail point.
    for (const point of trail) {
        const color = altitudeColor(point.altitude);
        const marker = L.circleMarker([point.lat, point.lon], {
            radius: 2,
            color,
            weight: 0,
            fillColor: color,
            fillOpacity: 1
        }).addTo(layer);
        marker.on('mouseover', () => marker.setStyle({ fillColor: '#ffffff', color: '#ffffff' }));
        marker.on('mouseout', () => marker.setStyle({ fillColor: color, color }));
    }
}

/**
 * Removes the current trail layer from the map.
 *
 * @param {L.Map} map
 * @returns {void}
 */
export function removeTrail(map) {
    if (layer) {
        map.removeLayer(layer);
        layer = null;
    }
}
