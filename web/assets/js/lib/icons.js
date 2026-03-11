// Copy / check icons for the info panel.
export const copyIcon = `
    <svg xmlns="http://www.w3.org/2000/svg" width="14" height="14" viewBox="0 0 448 512"
        fill="currentColor" aria-hidden="true">
        <path d="M384 336H192c-8.8 0-16-7.2-16-16V64c0-8.8 7.2-16 16-16l140.1 0L400 115.9V320c0 8.8-7.2
            16-16 16zM192 384h192c35.3 0 64-28.7 64-64V115.9c0-12.7-5.1-24.9-14.1-33.9L366 14.1A48 48 0
            0 0 332.1 0H192c-35.3 0-64 28.7-64 64v256c0 35.3 28.7 64 64 64zM64 128c-35.3 0-64 28.7-64
            64v256c0 35.3 28.7 64 64 64h192c35.3 0 64-28.7 64-64v-32h-48v32c0 8.8-7.2 16-16 16H64c-8.8
            0-16-7.2-16-16V192c0-8.8 7.2-16 16-16h32v-48H64z"/>
    </svg>`;

export const checkIcon = `
    <svg xmlns="http://www.w3.org/2000/svg" width="14" height="14" viewBox="0 0 448 512"
        fill="currentColor" aria-hidden="true">
        <path d="M438.6 105.4c12.5 12.5 12.5 32.8 0 45.3l-256 256c-12.5 12.5-32.8 12.5-45.3
            0l-128-128c-12.5-12.5-12.5-32.8 0-45.3s32.8-12.5 45.3 0L160 338.7 393.4 105.4c12.5-12.5
            32.8-12.5 45.3 0z"/>
    </svg>`;

// Aircraft silhouette icons for map markers. All use a 64x64 viewBox, rendered at 32x32.

// Generic single-aisle jet.
export const jet = `
    <svg xmlns="http://www.w3.org/2000/svg" width="32" height="32" viewBox="0 0 64 64"
        fill="currentColor" aria-hidden="true">
        <path d="m 32,1 2,1 2,3 0,18 4,1 0,-4 3,0 0,5 17,6 0,3 -15,-2 -9,0 0,12 -2,6 7,3 0,2 -8,-1
            -1,2 -1,-2 -8,1 0,-2 7,-3 -2,-6 0,-12 -9,0 -15,2 0,-3 17,-6 0,-5 3,0 0,4 4,-1 0,-18 2,-3
            2,-1z"/>
    </svg>`;

// Four-engine heavy jet.
export const jetHeavy = `
    <svg xmlns="http://www.w3.org/2000/svg" width="32" height="32" viewBox="0 0 64 64"
        fill="currentColor" aria-hidden="true">
        <path d="m 32,1 2,1 2,3 0,12 3,2 0,-3 2,0 0,4 4,3 0,-3 2,0 0,4 15,8 0,3 -15,-3 -6,0 0,1 -6,0
            0,10 -1,4 8,4 0,2 -9,-2 -1,2 -1,-2 -9,2 0,-2 8,-4 -1,-4 0,-10 -6,0 0,-1 -6,0 -15,3 0,-3
            15,-8 0,-4 2,0 0,3 4,-3 0,-4 2,0 0,3 3,-2 0,-12 2,-3 2,-1z"/>
    </svg>`;

// Private / business jet.
export const jetPrivate = `
    <svg xmlns="http://www.w3.org/2000/svg" width="32" height="32" viewBox="0 0 64 64"
        fill="currentColor" aria-hidden="true">
        <path d="m 32,1 1,0 1,2 1,4 0,10 21,17 0,5 -2,-2 -16,-8 -3,0 0,3 2,0 1,1 0,5 -1,1 0,3 -2,0
            0,1 7,5 0,3 -9,-3 -1,0 -9,3 0,-3 7,-5 0,-1 -2,0 0,-3 -1,-1 0,-5 1,-1 2,0 0,-3 -3,0 -16,8
            -2,2 0,-5 21,-17 0,-10 1,-4 1,-2z"/>
    </svg>`;

// Light single-engine prop.
export const propSingle = `
    <svg xmlns="http://www.w3.org/2000/svg" width="32" height="32" viewBox="0 0 64 64"
        fill="currentColor" aria-hidden="true">
        <path d="M 30.5,4 30.875,3 31.125,2.375 31.5,2 31.875,2.375 32.125,3 32.5,4 l 0.75,0.125
            0.625,0.5 0.125,0.625 0.5,4.25 0.125,3 11.5,0 15.5,0.75 0.625,0.5 0.25,0.75 0,5 -16.5,2
            -11.5,0 -2,15.875 8.25,1.5 0.5,0.375 0.125,0.375 -0.125,3 -0.125,0.375 -0.375,0.25
            -7.875,1 -1,-2.25 -0.25,4.75 -0.125,0.125 -0.125,-0.125 -0.25,-4.75 -1,2.25 -7.875,-1
            -0.375,-0.25 -0.125,-0.375 -0.125,-3 0.125,-0.375 0.5,-0.375 8.25,-1.5 -2,-15.875 -11.5,0
            -16.5,-2 0,-5 0.25,-0.75 0.625,-0.5 15.45,-0.75 11.55,0 0.125,-3 0.5,-4.25 0.125,-0.625
            0.625,-0.5 z"/>
    </svg>`;

// Small twin turboprop.
export const propTwin = `
    <svg xmlns="http://www.w3.org/2000/svg" width="32" height="32" viewBox="0 0 64 64"
        fill="currentColor" aria-hidden="true">
        <path d="m 32,1 1,0 1,2 1,4 0,5 5,0 0,-5 -1,-1 2,-2 2,2 -1,1 0,5 17,2 0,3 -17,3 0,1 -2,0
            0,-1 -5,0 0,5 -2,8 6,3 0,2 -6,-1 -1,0 -6,1 0,-2 6,-3 -2,-8 0,-5 -5,0 0,1 -2,0 0,-1
            -17,-3 0,-3 17,-2 0,-5 -1,-1 2,-2 2,2 -1,1 0,5 5,0 0,-5 1,-4 1,-2 z"/>
    </svg>`;

// Glider.
export const glider = `
    <svg xmlns="http://www.w3.org/2000/svg" width="32" height="32" viewBox="0 0 64 64"
        fill="currentColor" aria-hidden="true">
        <path d="m 31,1 1,0 1,2 1,4 1,6 0,3 16.5,0 11,2 1,2 -21,2 -8,0 -1,5 -1,15 0,4 4,0 5,1 0.5,1
            0,1 -11,0 0.5,2 0.5,-2 -11,0 0.5,-1 0,-1 5,-1 4,0 0,-4 -1,-15 -1,-5 -8,0 -21,-2 1,-2
            11,-2 16.5,0 0,-3 1,-6 1,-4 1,-2 z"/>
    </svg>`;

// Balloon.
export const balloon = `
    <svg xmlns="http://www.w3.org/2000/svg" width="32" height="32" viewBox="0 0 64 64"
        fill="currentColor" aria-hidden="true">
        <path d="m 27,1 10,0 3,1 3,1 1,1 2,1 6,6 1,2 1,1 1,3 1,3 0,10 -1,3 -1,3 -1,1 -1,2 -6,6 -2,1
            -1,1 -2,1 -2,1 -2,8 -1,0 2,-8 -3,1 -6,0 -3,-1 2,8 9,0 0,6 -10,0 0,-6 -2,-8 -2,-1 -2,-1
            -1,-1 -2,-1 -6,-6 -1,-2 -1,-1 -1,-3 -1,-3 0,-10 1,-3 1,-3 1,-1 1,-2 6,-6 2,-1 1,-1 3,-1
            3,-1z"/>
    </svg>`;

// Helicopter.
export const helicopter = `
    <svg xmlns="http://www.w3.org/2000/svg" width="32" height="32" viewBox="0 0 64 64"
        fill="currentColor" aria-hidden="true">
        <path d="m 32,6 3,2 2,4 0,8 -1,2 -2,1 0,24 -2,3 -2,-3 0,-24 -2,-1 -1,-2 0,-8 2,-4 3,-2z
            M 8,25 l 48,-24 0,2 -48,24z M 8,1 l 48,24 0,2 -48,-24z
            M 23,46 l 18,0 0,2 -18,0z"/>
    </svg>`;


// Quadcopter drone / UAV.
export const drone = `
    <svg xmlns="http://www.w3.org/2000/svg" width="32" height="32" viewBox="0 0 64 64"
        fill="currentColor" aria-hidden="true">
        <path d="m 24,10 16,0 0,16 -6,0 0,4 4,0 0,-6 16,0 0,16 -16,0 0,-6 -4,0 0,4 6,0 0,16 -16,0
            0,-16 6,0 0,-4 -4,0 0,6 -16,0 0,-16 16,0 0,6 4,0 0,-4 -6,0 0,-16z"/>
    </svg>`;

// Ground vehicle.
export const groundVehicle = `
    <svg xmlns="http://www.w3.org/2000/svg" width="32" height="32" viewBox="0 0 64 64"
        fill="currentColor" aria-hidden="true">
        <path d="m 28,4 8,0 2,2 0,6 2,0 0,8 -2,0 0,12 2,0 0,8 -2,0 0,6 -2,2 -8,0 -2,-2 0,-6 -2,0
            0,-8 2,0 0,-12 -2,0 0,-8 2,0 0,-6 2,-2z"/>
    </svg>`;

// Fixed obstacle / point.
export const obstacle = `
    <svg xmlns="http://www.w3.org/2000/svg" width="32" height="32" viewBox="0 0 64 64"
        fill="currentColor" aria-hidden="true">
        <path d="m 32,8 24,24 -24,24 -24,-24 24,-24z"/>
    </svg>`;

