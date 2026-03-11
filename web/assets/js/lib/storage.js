// Default map configuration.
const defaultMapView = { lat: 37.0, lng: 13.0, zoom: 8 };

// Persisted browser state for the web UI.
const mapViewStorageKey = 'dump1090.mapView';
const panelPositionStorageKey = 'dump1090.panelPosition';

/**
 * Loads the persisted map view from local storage.
 *
 * @returns {{lat: number, lng: number, zoom: number}}
 */
export function loadMapView() {
    try {
        const storedView = localStorage.getItem(mapViewStorageKey);
        if (!storedView) return defaultMapView;

        const parsedView = JSON.parse(storedView);
        const lat = Number(parsedView?.lat);
        const lng = Number(parsedView?.lng);
        const zoom = Number(parsedView?.zoom);
        const hasValidCoords = Number.isFinite(lat) && Number.isFinite(lng);
        const hasValidZoom = Number.isFinite(zoom);

        if (!hasValidCoords || !hasValidZoom) return defaultMapView;

        return { lat, lng, zoom };
    } catch (error) {
        console.warn('Error loading saved map view:', error);
        return defaultMapView;
    }
}

/**
 * Persists the current map center and zoom level.
 *
 * @param {L.Map} map
 * @returns {void}
 */
export function saveMapView(map) {
    try {
        const center = map.getCenter();
        localStorage.setItem(mapViewStorageKey, JSON.stringify({
            lat: center.lat,
            lng: center.lng,
            zoom: map.getZoom()
        }));
    } catch (error) {
        console.warn('Error saving map view:', error);
    }
}

/**
 * Loads the persisted panel position from local storage.
 *
 * @returns {{x: number, y: number}}
 */
export function loadPanelPosition() {
    try {
        const stored = localStorage.getItem(panelPositionStorageKey);
        if (!stored) return { x: 0, y: 0 };

        const parsed = JSON.parse(stored);
        const x = Number(parsed?.x);
        const y = Number(parsed?.y);
        if (!Number.isFinite(x) || !Number.isFinite(y)) return { x: 0, y: 0 };

        return { x, y };
    } catch (error) {
        console.warn('Error loading saved panel position:', error);
        return { x: 0, y: 0 };
    }
}

/**
 * Persists the panel position.
 *
 * @param {number} x
 * @param {number} y
 * @returns {void}
 */
export function savePanelPosition(x, y) {
    try {
        localStorage.setItem(panelPositionStorageKey, JSON.stringify({ x, y }));
    } catch (error) {
        console.warn('Error saving panel position:', error);
    }
}
