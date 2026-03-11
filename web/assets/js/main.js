import { loadMapView, saveMapView } from './lib/storage.js';
import { getAircraftIcon, updateAircraftMarkerRotation } from './aircraft.js';
import {
    drawTrail, removeTrail,
    distanceNM, angleDifference,
    HEADING_CHANGE_DEG, MAX_DISTANCE_NM, MIN_DISTANCE_NM, ALT_CHANGE_FT
} from './trail.js';
import {
    initPanel,
    updateAircraftInfo,
    updateGeneralInfo,
    showSelection,
    hideSelection
} from './panel.js';

// Application configuration.
const dataRefreshIntervalMs = 100;

// Runtime state for the current browser session.
let map = null;
let selectedAircraft = null;
const aircraftList = {};
let aircraftCount = 0;

/**
 * Clears the current aircraft selection and resets the panel.
 *
 * @returns {void}
 */
function clearSelectedAircraft() {
    const previousAircraft = aircraftList[selectedAircraft?.hex];
    if (previousAircraft) {
        previousAircraft.marker.setIcon(getAircraftIcon(previousAircraft, null));
    }
    selectedAircraft = null;
    removeTrail(map);
    hideSelection();
}

/**
 * Selects an aircraft and refreshes its marker and panel state.
 *
 * @param {{hex: string, marker: L.Marker}} aircraft
 * @returns {void}
 */
function selectAircraft(aircraft) {
    if (!aircraft) return;

    const previousAircraft = aircraftList[selectedAircraft?.hex];
    selectedAircraft = aircraft;
    showSelection();

    if (previousAircraft) {
        previousAircraft.marker.setIcon(getAircraftIcon(previousAircraft, selectedAircraft?.hex));
    }
    aircraft.marker.setIcon(getAircraftIcon(aircraft, selectedAircraft?.hex));
    updateAircraftInfo(aircraft);
    drawTrail(map, aircraft);
}

/**
 * Removes aircraft that are no longer present in the latest payload
 *
 * @param {Record<string, boolean>} seenAircraftHexes
 * @returns {void}
 */
function removeStaleAircraft(seenAircraftHexes) {
    for (const aircraft of Object.values(aircraftList)) {
        if (seenAircraftHexes[aircraft.hex]) continue;

        map.removeLayer(aircraft.marker);
        delete aircraftList[aircraft.hex];
        aircraftCount--;

        if (selectedAircraft?.hex === aircraft.hex) {
            clearSelectedAircraft();
        }
    }
}

/**
 * Creates or updates a map marker for a single aircraft.
 *
 * @param {{
 *   hex: string,
 *   flight?: string,
 *   altitude?: number | null,
 *   speed?: number | null,
 *   lat: number,
 *   lon: number,
 *   track?: number | null,
 *   marker?: L.Marker
 * }} aircraft
 * @returns {void}
 */
function upsertAircraft(aircraft) {
    // Normalize the incoming flight identifier.
    aircraft.flight = aircraft.flight?.trim() || '';

    // Register aircraft we have not seen yet.
    const existingAircraft = aircraftList[aircraft.hex];
    if (existingAircraft == null) {
        aircraft.trail = [{ lat: aircraft.lat, lon: aircraft.lon, altitude: aircraft.altitude, track: aircraft.track ?? null, timestamp: Date.now() }];
        aircraft.marker = L.marker([aircraft.lat, aircraft.lon], {
            icon: getAircraftIcon(aircraft, selectedAircraft?.hex),
            keyboard: false
        }).addTo(map);
        aircraft.marker.on('click', (event) => {
            L.DomEvent.stopPropagation(event);
            selectAircraft(aircraftList[aircraft.hex]);
        });
        aircraftList[aircraft.hex] = aircraft;
        aircraftCount++;
        return;
    }

    // Move the existing marker to the latest reported position.
    existingAircraft.marker.setLatLng([aircraft.lat, aircraft.lon]);

    // Rebuild the icon only when its structure changes.
    if (existingAircraft.emitter_category !== aircraft.emitter_category) {
        existingAircraft.marker.setIcon(getAircraftIcon(aircraft, selectedAircraft?.hex));
    } else {
        updateAircraftMarkerRotation(existingAircraft.marker, aircraft.emitter_category, aircraft.track);
    }

    // Adaptive trail sampling: more dots in turns, fewer on straight legs.
    const lastTrailPoint = existingAircraft.trail[existingAircraft.trail.length - 1];
    if (aircraft.lat !== lastTrailPoint.lat || aircraft.lon !== lastTrailPoint.lon) {
        const dist = distanceNM(lastTrailPoint.lat, lastTrailPoint.lon, aircraft.lat, aircraft.lon);

        if (dist >= MIN_DISTANCE_NM) {
            const headingDelta = angleDifference(lastTrailPoint.track, aircraft.track);
            const altDelta = Math.abs((aircraft.altitude ?? 0) - (lastTrailPoint.altitude ?? 0));
            const shouldRecord =
                headingDelta >= HEADING_CHANGE_DEG ||
                dist >= MAX_DISTANCE_NM ||
                altDelta >= ALT_CHANGE_FT;

            if (shouldRecord) {
                existingAircraft.trail.push({
                    lat: aircraft.lat,
                    lon: aircraft.lon,
                    altitude: aircraft.altitude,
                    track: aircraft.track ?? null,
                    timestamp: Date.now()
                });
            }
        }
    }

    // Update the cached aircraft state with the latest values from the feed.
    existingAircraft.altitude = aircraft.altitude;
    existingAircraft.speed = aircraft.speed;
    existingAircraft.lat = aircraft.lat;
    existingAircraft.lon = aircraft.lon;
    existingAircraft.track = aircraft.track;
    existingAircraft.flight = aircraft.flight;
    existingAircraft.emitter_category = aircraft.emitter_category;

    // Keep the details panel and trail in sync when this aircraft is currently selected.
    if (existingAircraft.hex === selectedAircraft?.hex) {
        updateAircraftInfo(existingAircraft);
        drawTrail(map, existingAircraft);
    }
}

/**
 * Fetches the latest aircraft list and syncs markers and panel state.
 *
 * @returns {Promise<void>}
 */
async function fetchData() {
    try {
        const response = await fetch('/data.json');
        if (!response.ok) return;

        const data = await response.json();
        const seenAircraftHexes = {};
        console.log(data);
        for (const aircraft of data) {
            if (aircraft.hex == null || aircraft.lat == null || aircraft.lon == null) continue;

            seenAircraftHexes[aircraft.hex] = true;
            upsertAircraft(aircraft);
        }

        removeStaleAircraft(seenAircraftHexes);
    } catch (error) {
        console.error('Error fetching data:', error);
    }
}

/**
 * Fetches the latest aircraft data and updates the general info panel.
 *
 * @returns {void}
 */
function refreshData() {
    fetchData();
    updateGeneralInfo(aircraftCount);
}

// Initialize the info panel.
initPanel({ onClearSelection: clearSelectedAircraft });

// Initialize the map instance.
const initialView = loadMapView();
map = L.map('canvas').setView([initialView.lat, initialView.lng], initialView.zoom);
L.tileLayer('https://tile.openstreetmap.org/{z}/{x}/{y}.png', {
    maxZoom: 19,
    attribution: '&copy; <a href="https://www.openstreetmap.org/copyright">OpenStreetMap</a>'
}).addTo(map);

// Clear selection on Escape.
addEventListener('keydown', (event) => {
    if (event.key === 'Escape' && selectedAircraft) clearSelectedAircraft();
});

// Setup map view persistence.
map.on('moveend', () => saveMapView(map));
addEventListener('beforeunload', () => saveMapView(map));

// Run the refresh loop.
refreshData();
setInterval(refreshData, dataRefreshIntervalMs);
