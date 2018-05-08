'use strict';

const Miner = require("./lib/0xbitcoinminer-accel");

init();

function init() {
    let os = require('os');
    global.oldWindows = process.platform == 'win32' && (os.release().slice(0,2) < 10 || os.release().slice(5,10) < 14392);

    let fs = require('fs');
    global.jsConfig = JSON.parse(fs.readFileSync('0xbitcoin.json'));

    if (!jsConfig)
    {
        console.print('Configuration file missing.');
        process.exit(1);
    }
    if (!jsConfig.hasOwnProperty('address') || !jsConfig.hasOwnProperty('pool'))
    {
        console.print('Faulty configuration file.');
        process.exit(1);
    }
    initSignalHandlers();

    Miner.init();
    Miner.mine();
}

function sigHandler(signal) {
    process.exit(128 + signal)
}

function initSignalHandlers(oldWindows) {
    process.on('SIGTERM', sigHandler);
    process.on('SIGINT', sigHandler);
    process.on('SIGBREAK', sigHandler);
    process.on('SIGHUP', sigHandler);
    process.on('SIGWINCH', (sig) => {
        if(!oldWindows)
            process.stdout.write("\x1b[5r\x1b[5;1f");
    });
    process.on('exit', (sig) => {
        if(!oldWindows)
            process.stdout.write("\x1b[s\x1b[?25h\x1b[r\x1b[u");
    });
}
