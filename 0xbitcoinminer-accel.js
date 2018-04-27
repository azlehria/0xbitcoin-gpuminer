'use strict';

const web3utils = require('web3-utils');
const BN = require('bn.js');
const miningLogger = require("./lib/mining-logger");
const tokenContractJSON = require('./contracts/_0xBitcoinToken.json');
const CPPMiner = require('./build/Release/hybridminer');

//only load this if selecting 'gpu mine!!!'

var tokenContract;

const PRINT_STATS_TIMEOUT = 100;
const PRINT_STATS_BARE_TIMEOUT = 5000;
const COLLECT_MINING_PARAMS_TIMEOUT = 4000;
var hardwareType = 'cuda'; //default
var startTime;
var newSolution = false;
var addressFrom;

module.exports = {
    async init(miningLogger)
    {
        process.on('exit', () => {
            miningLogger.print("Process exiting... stopping miner");
            CPPMiner.stop();
        });

        CPPMiner.setHardwareType('cuda')

        this.miningLogger = miningLogger;
    },

    async mine() {
        let miningParameters = {};
        await this.collectMiningParameters(miningParameters);

        if (!this.mining) {
            try {
                // C++ module entry point
                this.mineCoins(miningParameters);
            } catch (e) {
                miningLogger.print(e)
            }
        }

        //keep on looping!
        setInterval(async() => {
            await this.collectMiningParameters(miningParameters)
        }, COLLECT_MINING_PARAMS_TIMEOUT);

        let os = require('os');
        if(oldWindows) {
            setInterval(() => { this.printMiningStatsBare() }, PRINT_STATS_BARE_TIMEOUT);
        } else {
            setInterval(() => { this.printMiningStats() }, PRINT_STATS_TIMEOUT);
            //setInterval(() => { CPPMiner.printStatus() }, PRINT_STATS_TIMEOUT);
        }
    },

    async collectMiningParameters(miningParameters) {
        try {
            var parameters = await this.networkInterface.collectMiningParameters(miningParameters);

            miningParameters.miningDifficulty = parameters.miningDifficulty;
            miningParameters.challengeNumber = parameters.challengeNumber;
            miningParameters.miningTarget = parameters.miningTarget;
            miningParameters.poolEthAddress = parameters.poolEthAddress;

            //give data to the c++ addon
            await this.updateCPUAddonParameters(miningParameters)
        } catch (e) {
            miningLogger.print(e)
        }
    },

    async updateCPUAddonParameters(miningParameters) {
        if (this.challengeNumber == null || this.challengeNumber != miningParameters.challengeNumber) {
            this.challengeNumber = miningParameters.challengeNumber

            CPPMiner.setPrefix(this.challengeNumber + miningParameters.poolEthAddress.slice(2));
        }

        if (this.miningTarget == null || this.miningTarget != miningParameters.miningTarget) {
            this.miningTarget = miningParameters.miningTarget

            CPPMiner.setTarget("0x" + this.miningTarget.toString(16, 64));
        }

        if (this.miningDifficulty == null || this.miningDifficulty != miningParameters.miningDifficulty) {
            this.miningDifficulty = miningParameters.miningDifficulty

            CPPMiner.setDiff("0x" + this.miningDifficulty);
            // CPPMiner.setDifficulty( parseInt( this.miningTarget.toString(16, 64).substring(0, 16), 16 ) );
        }
    },

    mineCoins(miningParameters) {
        const verifyAndSubmit = () => {
            let solution_number = "0x" + CPPMiner.getSolution();
            if (solution_number == "0x" || web3utils.toBN(solution_number).eq(0)) { return; }
            let challenge_number = miningParameters.challengeNumber;
            let digest = web3utils.soliditySha3(challenge_number,
                                                miningParameters.poolEthAddress,
                                                solution_number);
            let digestBigNumber = web3utils.toBN(digest);
            if (digestBigNumber.lte(miningParameters.miningTarget)) {
                this.submitNewMinedBlock(addressFrom, solution_number, digest, challenge_number,
                                         miningParameters.miningTarget, miningParameters.miningDifficulty)
            //} else {
            //    console.error("Verification failed!\n",
            //                  "challenge:", challenge_number, "\n",
            //                  "address:", addressFrom, "\n",
            //                  "solution:", solution_number, "\n",
            //                  "digest:", digest, "\n",
            //                  "target:", miningParameters.miningTarget);
            }
        }

        setInterval(() => { verifyAndSubmit() }, 500);

        this.mining = true;

        startTime = Date.now();

        CPPMiner.run(() => {});
    },

    resetHashCounter() {
        CPPMiner.resetHashCounter();

        startTime = Date.now();
    },

    setNetworkInterface(netInterface) {
        this.networkInterface = netInterface;
        this.submitNewMinedBlock = netInterface.queueMiningSolution;
        netInterface.setResetCallback( this.resetHashCounter );
    },

    printMiningStats() {
        let hashes = "0x" + CPPMiner.getGpuHashes() || 0;
        let timeDiff = ((Date.now() - startTime) / 1000) || 0.100;

        if(typeof this.avgHashes == 'undefined') {
            this.avgHashes = (hashes / timeDiff);
            this.samples = 1;
        } else {
            this.samples++;
            if((this.samples > 600 &&
                ((hashes/timeDiff) > (1.50 * this.avgHashes) ||
                 (hashes/timeDiff) < (0.75 * this.avgHashes))) ||
               (this.samples > 150 &&
                ((hashes/timeDiff) > (2.50 * this.avgHashes) ||
                 (hashes/timeDiff) < (0.50 * this.avgHashes))) ||
               (this.samples > 50 &&
                ((hashes/timeDiff) > (3.00 * this.avgHashes) ||
                 (hashes/timeDiff) < (0.50 * this.avgHashes))) ||
               isNaN(hashes))
            {
                if(isNaN(hashes)) console.log("wtf")
                hashes = this.avgHashes * timeDiff;
            }
            this.avgHashes -= this.avgHashes / this.samples;
            this.avgHashes += (hashes / timeDiff) / this.samples;
        }

        process.stdout.cork();
        process.stdout.write("\x1b[s\x1b[?25l\x1b[2;22f\x1b[38;5;221m" +
                             (this.avgHashes / 1000000).toFixed(2).toString().padStart(8).slice(-8) +
                             "\x1b[0m\x1b[3;36f\x1b[38;5;208m" +
                             (hashes/1).toFixed(0).toString().replace(/\B(?=(\d{3})+(?!\d))/g, ",").padStart(25) +
                             "\x1b[0m\x1b[2;75f\x1b[38;5;33m" +
                             Math.floor((timeDiff) / 60).toFixed(0).toString().padStart(2, '0') +
                             ":" +
                             Math.floor((timeDiff) % 60).toFixed(0).toString().padStart(2, '0') +
                             "\x1b[0m");

        process.stdout.write("\x1b[2;13f\x1b[38;5;34m" +
                             this.challengeNumber.substring(2, 10) +
                             "\x1b[0m");

        process.stdout.write("\x1b[3;14f\x1b[38;5;34m" +
                             this.miningDifficulty.toString().padEnd(7) +
                             "\x1b[0m");

        process.stdout.write("\x1b[3;22f\x1b[38;5;221m" +
                             this.networkInterface.getSolutionCount().toString().padStart(8) +
                             "\x1b[0m");

        process.stdout.write('\x1b[3;72f\x1b[38;5;33m' +
                             jsConfig.address.slice(0, 8) +
                             '\x1b[0m\x1b[u\x1b[?25h');
        process.stdout.uncork();
    },

    printMiningStatsBare() {
        let hashes = "0x" + CPPMiner.getGpuHashes();
        let timeDiff = ((Date.now() - startTime) / 1000.0) || 0.100;
        if(typeof this.avgHashes == 'undefined') {
            this.avgHashes = (hashes / timeDiff);
            this.samples = 1;
        } else {
            this.samples++;
            if((this.samples > 600 &&
                ((hashes/timeDiff) > (1.50 * this.avgHashes) ||
                 (hashes/timeDiff) < (0.75 * this.avgHashes))) ||
               (this.samples > 150 &&
                ((hashes/timeDiff) > (2.50 * this.avgHashes) ||
                 (hashes/timeDiff) < (0.50 * this.avgHashes))) ||
               (this.samples > 50 &&
                ((hashes/timeDiff) > (3.00 * this.avgHashes) ||
                 (hashes/timeDiff) < (0.50 * this.avgHashes))) ||
               isNaN(hashes))
            {
                if(isNaN(hashes)) console.log("wtf")
                hashes = this.avgHashes * timeDiff;
            }
            this.avgHashes -= this.avgHashes / this.samples;
            this.avgHashes += (hashes / timeDiff) / this.samples;
        }

        miningLogger.print(/*'Raw Hashes:',
                           (hashes/1).toFixed(0).toString().replace(/\B(?=(\d{3})+(?!\d))/g, ",").padStart(16),
                           'Hash rate:',*/
                           (this.avgHashes / 1000000).toFixed(2).toString().replace(/\B(?=(\d{3})+(?!\d))/g, ",").padStart(10).slice(-10),
                           "MH/s  Sols:",
                           this.networkInterface.getSolutionCount().toString().replace(/\B(?=(\d{3})+(?!\d))/g, ",").padStart(6)
                           + (newSolution ? '^' : ' '),
                           "Search time:",
                           Math.floor((timeDiff) / 60).toFixed(0).toString().padStart(2, '0') +
                           ":" +
                           Math.floor((timeDiff) % 60).toFixed(0).toString().padStart(2, '0'));
        newSolution = false;
    }
}
