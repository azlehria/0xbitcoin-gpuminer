'use strict';

const Miner = require("./lib/0xbitcoinminer-accel");

init();

function init() {
    Miner.init();
    Miner.mine();
}
