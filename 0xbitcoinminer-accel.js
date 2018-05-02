'use strict';

const web3utils = require('web3-utils');
const miningLogger = require("./lib/mining-logger");
const CPPMiner = require('./build/Release/hybridminer');

const PRINT_STATS_TIMEOUT = 100;
const PRINT_STATS_BARE_TIMEOUT = 5000;
const COLLECT_MINING_PARAMS_TIMEOUT = 4000;
var startTime;
var newSolution = false;
var addressFrom;
var oldChallenge;
var failedSolutions = 0;
var hashAverage = 0;
var hashSamples = 0;

module.exports = {
    async init()
    {
        process.on('exit', () => {
            miningLogger.print("Process exiting... stopping miner");
            CPPMiner.stop();
        });

        CPPMiner.setHardwareType('cuda')
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
            oldChallenge = this.challengeNumber;
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

    async mineCoins(miningParameters) {
        const verifyAndSubmit = () => {
            let solution_number = "0x" + CPPMiner.getSolution();
            if (solution_number == "0x" || web3utils.toBN(solution_number).eq(0)) { return; }
            let challenge_number = miningParameters.challengeNumber;
            let digest = web3utils.soliditySha3(challenge_number,
                                                miningParameters.poolEthAddress,
                                                solution_number);
            let digestBigNumber = web3utils.toBN(digest);
            if (digestBigNumber.lte(miningParameters.miningTarget)) {
                this.submitNewMinedBlock(solution_number,
                                         digest,
                                         challenge_number,
                                         miningParameters.miningTarget,
                                         miningParameters.miningDifficulty)
            } else {
                if (oldChallenge && web3utils.toBN(web3utils.soliditySha3(oldChallenge,
                                                                          miningParameters.poolEthAddress,
                                                                          solution_number)).lte(miningParameters.miningTarget)) {
                    miningLogger.print("CPU verification failed: stale solution.");
                    if (jsConfig.submitstale) {
                        miningLogger.print("               Submitting solution anyway.")
                        this.submitNewMinedBlock(solution_number,
                                                 digest,
                                                 challenge_number,
                                                 miningParameters.miningTarget,
                                                 miningParameters.miningDifficulty)
                    }
                } else {
                    failedSolutions++;
                    console.error("CPU verification failed:\n",
                                  " challenge:", challenge_number, "\n",
                                  " address:  ", miningParameters.poolEthAddress, "\n",
                                  " solution: ", solution_number, "\n",
                                  " digest:   ", digest, "\n",
                                  " target:   ", "0x" + miningParameters.miningTarget.toString(16, 64));
                }
            }
        }

        this.mining = true;

        CPPMiner.run(() => {});

        startTime = Date.now();

        setInterval(() => { verifyAndSubmit() }, 500);
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
        let hashes = parseInt(CPPMiner.getGpuHashes(), 16) || 0;
        let timeDiff = ((Date.now() - startTime) / 1000) || 0.100;

        hashSamples++;

        hashAverage -= hashAverage / hashSamples;
        hashAverage += (hashes / timeDiff) / hashSamples;

        process.stdout.cork();
        process.stdout.write("\x1b[s\x1b[?25l\x1b[2;22f\x1b[38;5;221m" +
                             (hashAverage / 1000000).toFixed(2).toString().padStart(8).slice(-8) +
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

        if (jsConfig["debug"] && failedSolutions > 0)
        {
            process.stdout.write("\x1b[4;22f\x1b[38;5;196m" +
                                 failedSolutions.toString().padStart(8) +
                                 "\x1b[0m");
        }

        process.stdout.write('\x1b[3;72f\x1b[38;5;33m' +
                             jsConfig.address.slice(0, 8) +
                             '\x1b[0m\x1b[u\x1b[?25h');
        process.stdout.uncork();
    },

    printMiningStatsBare() {
        let hashes = parseInt(CPPMiner.getGpuHashes(), 16) || 0;
        let timeDiff = ((Date.now() - startTime) / 1000) || 0.100;

        hashSamples++;

        hashAverage -= hashAverage / hashSamples;
        hashAverage += (hashes / timeDiff) / hashSamples;

        miningLogger.print(/*'Raw Hashes:',
                           (hashes/1).toFixed(0).toString().replace(/\B(?=(\d{3})+(?!\d))/g, ",").padStart(16),
                           'Hash rate:',*/
                           (hashAverage / 1000000).toFixed(2).toString().replace(/\B(?=(\d{3})+(?!\d))/g, ",").padStart(10).slice(-10),
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
