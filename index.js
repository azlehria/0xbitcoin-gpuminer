'use strict';

const Miner = require("./0xbitcoinminer-accel");
const miningLogger = require("./lib/mining-logger");
var prompt = require('prompt');
var pjson = require('./package.json');
var PoolInterface = require("./lib/pool-interface");

var running = true;

init();

function init() {
    let os = require('os');
    global.oldWindows = process.platform == 'win32' && (os.release().slice(0,2) < 10 || os.release().slice(5,10) < 14392);

    if (process.platform == 'win32') {
        global.jsConfig = require(process.execPath.substring(0, process.execPath.lastIndexOf('\\')) + '/0xbitcoin.json');
    } else {
        global.jsConfig = require(process.execPath.substring(0, process.execPath.lastIndexOf('/')) + '/0xbitcoin.json');
    }

    if (!jsConfig)
    {
        miningLogger('Configuration file missing.');
        process.exit(1);
    }
    if (!jsConfig.hasOwnProperty('address') || !jsConfig.hasOwnProperty('pool') || !jsConfig.hasOwnProperty('cuda'))
    {
        miningLogger('Faulty configuration file.');
        process.exit(1);
    }
    initSignalHandlers();

    prompt.message = null;
    prompt.delimiter = ":";
    prompt.start({ noHandleSIGINT: true });

    handleCommand(['pool']);
    drawLayout();
    return getPrompt();
}

async function getPrompt() {
    await promptForCommand().then(handleCommand,(err) => {
        throw err;
    }).catch((err) => {
        if(err.message == 'canceled') {
            process.emit('SIGINT');
        }
    });

    return await getPrompt();
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

function drawLayout() {
    if(oldWindows) return;

    process.stdout.write( "\x1b[?25l\x1b[2J\x1b(0" );
    process.stdout.write( "\x1b[1;1flqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqwqqqqqqqqqqqqqqqqqqqqqqqqqqwqqqqqqqqqqqqqqqqqk" );
    process.stdout.write( "\x1b[4;1fmqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqvqqqqqqqqqqqqqqqqqqqqqqqqqqvqqqqqqqqqqqqqqqqqj" );
    process.stdout.write( "\x1b[2;1fx\x1b[2;35fx\x1b[2;62fx\x1b[2;80fx" );
    process.stdout.write( "\x1b[3;1fx\x1b[3;35fx\x1b[3;62fx\x1b[3;80fx" );
    process.stdout.write( "\x1b(B\x1b[2;2fChallenge:" );
    process.stdout.write( "\x1b[3;2fDifficulty:" );
    process.stdout.write( "\x1b[2;37fHashes this round" );
    process.stdout.write( "\x1b[2;63fRound time:" );
    process.stdout.write( "\x1b[3;63fAccount:" );
    process.stdout.write( "\x1b[2;31fMH/s" );
    process.stdout.write( "\x1b[3;31fSols" );
    process.stdout.write( "\x1b[s\x1b[3;29f\x1b[38;5;221m0\x1b[0m\x1b[u" );
    process.stdout.write( "\x1b[1;64fv" + pjson.version );
    process.stdout.write( "\x1b[5r\x1b[5;1f\x1b[?25h" );
}

async function promptForCommand() {
    return new Promise((fulfilled, rejected) => {
        prompt.get(['command'], async(err, result) => {
            if (err) { return rejected(err); }
            if (typeof result == 'undefined') { return rejected(result); }

            return fulfilled(result.command.split(' '));
        });
    });
}

async function handleCommand(command) {
    var subsystem_name = command[0];

    if (subsystem_name == 'pool') {
        await PoolInterface.init(miningLogger);

        Miner.init(miningLogger);
        Miner.setNetworkInterface(PoolInterface);
        Miner.mine()
    }

    if (subsystem_name == 'exit' || subsystem_name == 'quit') {
        process.exit(0);
    }
}
