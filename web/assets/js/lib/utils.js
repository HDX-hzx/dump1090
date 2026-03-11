/**
 * Copies text to the clipboard
 *
 * @param {string} text
 * @returns {Promise<void>}
 */
export async function copyText(text) {
    if (!navigator.clipboard) {
        throw new Error('Clipboard API unavailable');
    }

    await navigator.clipboard.writeText(text);
}
