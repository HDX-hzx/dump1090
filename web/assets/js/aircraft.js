import {
    jet, jetHeavy, jetPrivate, propSingle, propTwin, glider,
    balloon, helicopter, drone, groundVehicle, obstacle
} from './lib/icons.js';

// Marker icons keyed by ADS-B emitter category. Unlisted categories fall back to jet.
const markerIcons = {
    9: groundVehicle,   // Surface Vehicle — Emergency
    10: groundVehicle,  // Surface Vehicle — Service
    11: obstacle,       // Point Obstacle
    12: obstacle,       // Cluster Obstacle
    13: obstacle,       // Line Obstacle
    17: glider,         // Glider or Sailplane
    18: balloon,        // Lighter Than Air
    19: obstacle,       // Parachute / Sky Diver — no good top-down silhouette
    20: propSingle,     // Ultralight Vehicle
    21: drone,          // UAV
    25: propSingle,     // Light Airplane
    26: propTwin,       // Small Airplane
    27: jet,            // Large Airplane
    28: jetHeavy,       // High Vortex Aircraft
    29: jetHeavy,       // Heavy Airplane
    30: jetPrivate,     // High Performance Aircraft
    31: helicopter,     // Rotorcraft
};

function getMarkerIcon(emitterCategory) {
    return markerIcons[emitterCategory] ?? jet;
}

// Categories whose icons are not top-down silhouettes and should not rotate with track.
const nonRotatingCategories = new Set([18, 19, 11, 12, 13]);

function shouldRotate(emitterCategory) {
    return !nonRotatingCategories.has(emitterCategory);
}

/* ADS-B emitter category labels keyed by the flattened emitter_category value
 * produced by the backend. Values 0..31 map directly to A0..A7, B0..B7,
 * C0..C7, D0..D7 in order.
 *
 * Public reference:
 * FAA AC 20-165B Appendix tables:
 * https://www.faa.gov/documentLibrary/media/Advisory_Circular/AC_20-165B.pdf */
const emitterCategoryLabels = [
    'No Emitter Category',
    'Light Airplane',
    'Small Airplane',
    'Large Airplane',
    'High Vortex Aircraft',
    'Heavy Airplane',
    'High Performance Aircraft',
    'Rotorcraft',
    'No Emitter Category',
    'Glider or sailplane',
    'Lighter Than Air',
    'Parachute / Sky Diver',
    'Ultralight Vehicle',
    'UAV',
    'Space/Trans-atmospheric Vehicle',
    'Reserved',
    'No Emitter Category',
    'Surface Vehicle—Emergency Vehicle',
    'Surface Vehicle—Service Vehicle',
    'Point Obstacle (Includes Tethered Balloons)',
    'Cluster Obstacle',
    'Line Obstacle',
    'Reserved',
    'Reserved',
    'No Emitter Category',
    'Reserved',
    'Reserved',
    'Reserved',
    'Reserved',
    'Reserved',
    'Reserved',
    'Reserved'
];

/**
 * Returns the display label for an emitter category code, or null if invalid.
 *
 * @param {number | null | undefined} emitterCategory
 * @returns {string | null}
 */
export function getEmitterCategoryLabel(emitterCategory) {
    if (!Number.isInteger(emitterCategory) || emitterCategory < 0 ||
        emitterCategory >= emitterCategoryLabels.length) {
        return null;
    }
    return emitterCategoryLabels[emitterCategory];
}

/**
 * Builds the marker icon for an aircraft based on its emitter category.
 *
 * @param {{hex: string, emitter_category?: number | null, track?: number | null}} aircraft
 * @param {string | null} selectedAircraftHex
 * @returns {L.DivIcon}
 */
export function getAircraftIcon(aircraft, selectedAircraftHex) {
    const rotation = shouldRotate(aircraft.emitter_category) && aircraft.track || 0;
    return L.divIcon({
        html: `
            <div
                class="aircraft-icon__symbol${
                    selectedAircraftHex === aircraft.hex ? ' aircraft-icon__symbol--selected' : ''
                }"
                style="transform: rotate(${rotation}deg);"
            >
                ${getMarkerIcon(aircraft.emitter_category)}
            </div>
        `,
        className: 'aircraft-icon',
        iconSize: [32, 32],
        iconAnchor: [16, 16]
    });
}

/**
 * Updates the rendered rotation for an aircraft marker without rebuilding its icon.
 *
 * @param {L.Marker} marker
 * @param {number | null | undefined} emitterCategory
 * @param {number | null | undefined} track
 * @returns {void}
 */
export function updateAircraftMarkerRotation(marker, emitterCategory, track) {
    const symbol = marker.getElement()?.querySelector('.aircraft-icon__symbol');
    if (!(symbol instanceof HTMLElement)) return;

    const rotation = shouldRotate(emitterCategory) && track || 0;
    symbol.style.transform = `rotate(${rotation}deg)`;
}
