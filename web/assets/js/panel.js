import { copyText } from './lib/utils.js';
import { copyIcon, checkIcon } from './lib/icons.js';
import { getEmitterCategoryLabel } from './aircraft.js';
import { loadPanelPosition, savePanelPosition } from './lib/storage.js';

const defaultAircraftInfoText = 'Select an aircraft to view details';
const copyButtonFadeDurationMs = 120;

// Cached DOM references.
const aircraftInfo = document.getElementById('aircraftInfo');
const generalInfo = document.getElementById('generalInfo');
const clearSelectionButton = document.getElementById('clearSelectionButton');
const infoPanel = document.getElementById('info-panel');

// Cached field references, keyed by field name.
// Each entry holds { value: HTMLElement, copyButton: HTMLButtonElement | null }.
let fields = {};

/**
 * Updates a copy button value and availability.
 *
 * @param {HTMLButtonElement | null} button
 * @param {string} displayValue
 * @returns {void}
 */
function updateCopyButton(button, displayValue) {
    if (!button) return;

    const hasValue = Boolean(displayValue && displayValue !== '—');
    button.dataset.copyValue = hasValue ? displayValue : '';
    button.hidden = !hasValue;
}

/**
 * Sets a field's displayed text and syncs its copy button.
 *
 * @param {string} name
 * @param {string} displayValue
 * @returns {void}
 */
function setField(name, displayValue) {
    const field = fields[name];
    if (!field) return;

    field.value.textContent = displayValue;
    updateCopyButton(field.copyButton, displayValue);
}

/**
 * Shows temporary visual feedback on a copied button.
 *
 * @param {HTMLButtonElement} button
 * @returns {void}
 */
function showCopiedFeedback(button) {
    const previousTimer = button._resetTimer;
    if (previousTimer !== undefined) {
        clearTimeout(previousTimer);
    }

    button.classList.add('info-panel__copy-button--copied');
    button.innerHTML = checkIcon;

    button._resetTimer = setTimeout(() => {
        button.classList.remove('info-panel__copy-button--copied');
        if (button.closest('.is-hovered') || button.classList.contains('is-focused')) {
            button.innerHTML = copyIcon;
        } else {
            setTimeout(() => {
                button.innerHTML = copyIcon;
            }, copyButtonFadeDurationMs);
        }
        button._resetTimer = undefined;
    }, 1250);
}

/**
 * Updates the aircraft count shown in the info panel.
 *
 * @param {number} aircraftCount
 * @returns {void}
 */
export function updateGeneralInfo(aircraftCount) {
    generalInfo.innerHTML = `
        <span class="info-panel__value">${aircraftCount}</span>
        <span class="info-panel__label">aircraft tracked</span>
    `;
}

/**
 * Renders the selected aircraft details in the info panel.
 *
 * @param {{
 *   flight?: string,
 *   hex: string,
 *   emitter_category?: number | null,
 *   altitude?: number | null,
 *   speed?: number | null,
 *   track?: number | null,
 *   lat: number,
 *   lon: number
 * }} aircraft
 * @returns {void}
 */
export function updateAircraftInfo(aircraft) {
    if (!aircraft) return;

    setField('flight', aircraft.flight || '—');
    setField('icao', aircraft.hex);
    setField('category', getEmitterCategoryLabel(aircraft.emitter_category) ?? '—');
    setField('altitude', aircraft.altitude != null ? `${aircraft.altitude} ft` : '—');
    setField('speed', aircraft.speed != null ? `${aircraft.speed} kts` : '—');
    setField('track', aircraft.track != null ? `${aircraft.track}°` : '—');
    setField('coords', `${aircraft.lat.toFixed(4)}, ${aircraft.lon.toFixed(4)}`);
}

/**
 * Switches the panel to the selected-aircraft view.
 *
 * @returns {void}
 */
export function showSelection() {
    aircraftInfo.classList.add('has-selection');
    clearSelectionButton.classList.remove('is-hidden');
}

/**
 * Switches the panel back to the default placeholder view.
 *
 * @returns {void}
 */
export function hideSelection() {
    aircraftInfo.classList.remove('has-selection');
    clearSelectionButton.classList.add('is-hidden');
}

/**
 * Initializes the info panel: dragging, copy-button interactions, and defaults.
 *
 * @param {{onClearSelection: () => void}} callbacks
 * @returns {void}
 */
export function initPanel({ onClearSelection }) {
    // Build the aircraft details template.
    aircraftInfo.innerHTML = `
        <p class="info-panel__placeholder">${defaultAircraftInfoText}</p>
        <div class="info-panel__details">
            <div class="info-panel__headline">
                <div class="info-panel__flight-group">
                    <div class="info-panel__flight" data-field="flight"></div>
                    <button type="button" class="info-panel__copy-button"
                        data-copy-for="flight" aria-label="Copy value" title="Copy">
                        ${copyIcon}
                    </button>
                </div>
                <div class="info-panel__subtitle">
                    <span class="info-panel__secondary-label">Emitter Category:</span>
                    <span data-field="category"></span>
                </div>
                <div class="info-panel__secondary-row">
                    <span class="info-panel__secondary-label">ICAO 24-bit Address:</span>
                    <span class="info-panel__secondary-value" data-field="icao"></span>
                    <button type="button" class="info-panel__copy-button"
                        data-copy-for="icao" aria-label="Copy value" title="Copy">
                        ${copyIcon}
                    </button>
                </div>
            </div>
            <div class="info-panel__grid">
                <div class="info-panel__item">
                    <div class="info-panel__label-row">
                        <span class="info-panel__label">Altitude</span>
                        <button type="button" class="info-panel__copy-button"
                            data-copy-for="altitude" aria-label="Copy value" title="Copy">
                            ${copyIcon}
                        </button>
                    </div>
                    <span class="info-panel__value" data-field="altitude"></span>
                </div>
                <div class="info-panel__item">
                    <div class="info-panel__label-row">
                        <span class="info-panel__label">Ground Speed</span>
                        <button type="button" class="info-panel__copy-button"
                            data-copy-for="speed" aria-label="Copy value" title="Copy">
                            ${copyIcon}
                        </button>
                    </div>
                    <span class="info-panel__value" data-field="speed"></span>
                </div>
                <div class="info-panel__item">
                    <div class="info-panel__label-row">
                        <span class="info-panel__label">Track</span>
                        <button type="button" class="info-panel__copy-button"
                            data-copy-for="track" aria-label="Copy value" title="Copy">
                            ${copyIcon}
                        </button>
                    </div>
                    <span class="info-panel__value" data-field="track"></span>
                </div>
                <div class="info-panel__item info-panel__item--wide">
                    <div class="info-panel__label-row">
                        <span class="info-panel__label">Coordinates</span>
                        <button type="button" class="info-panel__copy-button"
                            data-copy-for="coords" aria-label="Copy value" title="Copy">
                            ${copyIcon}
                        </button>
                    </div>
                    <span class="info-panel__value" data-field="coords"></span>
                </div>
            </div>
        </div>
    `;

    // Pair each data-field element with its matching data-copy-for button.
    for (const el of aircraftInfo.querySelectorAll('[data-field]')) {
        const name = el.dataset.field;
        fields[name] = {
            value: el,
            copyButton: aircraftInfo.querySelector(`[data-copy-for="${name}"]`)
        };
    }

    // Draggable info panel via pointer events.
    // The header acts as the drag handle. Pointer capture ensures moves
    // are tracked even when the cursor leaves the header bounds.
    // Panel position is tracked via panelX / panelY closure variables
    // and applied as a CSS transform so the layout position is preserved.
    const header = infoPanel.querySelector('.info-panel__header');
    let dragPointerId = null;
    let dragStartX = 0;
    let dragStartY = 0;
    let panelStartX = 0;
    let panelStartY = 0;
    let panelX = 0;
    let panelY = 0;

    // Cached during drag to avoid repeated getBoundingClientRect() calls.
    let clampMinX = 0;
    let clampMaxX = 0;
    let clampMinY = 0;
    let clampMaxY = 0;

    /**
     * Computes clamping bounds from the current viewport and panel size.
     *
     * @returns {void}
     */
    function updateClampBounds() {
        const rect = infoPanel.getBoundingClientRect();
        const baseLeft = rect.left - panelX;
        const baseTop = rect.top - panelY;
        clampMinX = -baseLeft;
        clampMaxX = window.innerWidth - baseLeft - rect.width;
        clampMinY = -baseTop;
        clampMaxY = window.innerHeight - baseTop - rect.height;
    }

    /**
     * Clamps an x/y offset so the panel stays within the viewport.
     *
     * @param {number} x
     * @param {number} y
     * @returns {{x: number, y: number}}
     */
    function clampPosition(x, y) {
        return {
            x: Math.max(clampMinX, Math.min(clampMaxX, x)),
            y: Math.max(clampMinY, Math.min(clampMaxY, y))
        };
    }

    /**
     * Applies a translate offset to the panel.
     *
     * @param {number} x
     * @param {number} y
     * @returns {void}
     */
    function applyPosition(x, y) {
        const clamped = clampPosition(x, y);
        panelX = clamped.x;
        panelY = clamped.y;
        infoPanel.style.transform = `translate(${panelX}px, ${panelY}px)`;
    }

    /**
     * Ends the current drag, persists position, and restores body styles.
     *
     * @returns {void}
     */
    function endDrag() {
        dragPointerId = null;
        header.style.cursor = '';
        document.body.style.cursor = '';
        document.body.style.userSelect = '';
        savePanelPosition(panelX, panelY);
    }

    // Restore persisted panel position.
    const savedPosition = loadPanelPosition();
    updateClampBounds();
    applyPosition(savedPosition.x, savedPosition.y);

    // Record the pointer and the panel's current offset when a drag begins.
    header.addEventListener('pointerdown', (event) => {
        if (dragPointerId != null) return;
        if (event.target.closest('button')) return;
        dragPointerId = event.pointerId;
        header.setPointerCapture(event.pointerId);
        dragStartX = event.clientX;
        dragStartY = event.clientY;
        panelStartX = panelX;
        panelStartY = panelY;
        updateClampBounds();
        header.style.cursor = 'grabbing';
        document.body.style.cursor = 'grabbing';
        document.body.style.userSelect = 'none';
    });

    // Move the panel by the delta between the current and starting pointer position.
    header.addEventListener('pointermove', (event) => {
        if (event.pointerId !== dragPointerId) return;
        const x = panelStartX + (event.clientX - dragStartX);
        const y = panelStartY + (event.clientY - dragStartY);
        applyPosition(x, y);
    });

    // Release the drag on pointer up or cancel.
    header.addEventListener('pointerup', (event) => {
        if (event.pointerId !== dragPointerId) return;
        endDrag();
    });

    header.addEventListener('pointercancel', (event) => {
        if (event.pointerId !== dragPointerId) return;
        endDrag();
    });

    // Nudge the panel back into view when the window is resized.
    let resizeTimer = null;
    addEventListener('resize', () => {
        clearTimeout(resizeTimer);
        resizeTimer = setTimeout(() => {
            updateClampBounds();
            applyPosition(panelX, panelY);
            savePanelPosition(panelX, panelY);
        }, 100);
    });

    // Handle copy buttons on label hover
    aircraftInfo.addEventListener('mouseover', (event) => {
        const hoverRow = event.target
            .closest('.info-panel__flight-group, .info-panel__secondary-row, .info-panel__label-row');
        if (!hoverRow) return;
        hoverRow.classList.add('is-hovered');
    });

    aircraftInfo.addEventListener('mouseout', (event) => {
        const hoverRow = event.target
            .closest('.info-panel__flight-group, .info-panel__secondary-row, .info-panel__label-row');
        if (!hoverRow) return;
        const relatedTarget = event.relatedTarget;
        if (relatedTarget instanceof Node && hoverRow.contains(relatedTarget)) return;
        hoverRow.classList.remove('is-hovered');
    });

    // Handle copy buttons on focus (e.g. keyboard navigation)
    aircraftInfo.addEventListener('focusin', (event) => {
        const copyButton = event.target.closest('.info-panel__copy-button');
        if (!copyButton) return;
        copyButton.classList.add('is-focused');
    });

    aircraftInfo.addEventListener('focusout', (event) => {
        const copyButton = event.target.closest('.info-panel__copy-button');
        if (!copyButton) return;
        copyButton.classList.remove('is-focused');
    });

    // Handle clear selection button
    clearSelectionButton.addEventListener('click', (event) => {
        event.preventDefault();
        onClearSelection();
    });

    // Handle copy buttons click
    aircraftInfo.addEventListener('click', async (event) => {
        const copyButton = event.target.closest('[data-copy-value]');
        if (!copyButton) return;

        event.preventDefault();
        const triggeredByMouse = event.detail > 0;

        try {
            await copyText(copyButton.dataset.copyValue);
            showCopiedFeedback(copyButton);
            if (triggeredByMouse) {
                copyButton.blur();
            }
        } catch (error) {
            console.error('Error copying value:', error);
        }
    });

    // Set defaults
    hideSelection();
}
