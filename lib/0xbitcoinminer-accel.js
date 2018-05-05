'use strict';

const web3utils = require('web3-utils');
const CPPMiner = require('../build/Release/hybridminer.node');
const networkInterface = require("./pool-interface");

const PRINT_STATS_TIMEOUT = 100;
const PRINT_STATS_BARE_TIMEOUT = 5000;
const COLLECT_MINING_PARAMS_TIMEOUT = 4000;
var newSolution = false;
var addressFrom;
var oldChallenge;
var failedSolutions = 0;

module.exports = {
    init()
    {
        this.submitNewMinedBlock = networkInterface.queueMiningSolution;
        networkInterface.init(CPPMiner.resetHashCounter, CPPMiner.incSolCount, CPPMiner.log);

        process.on('exit', () => {
            CPPMiner.log("Process exiting... stopping miner");
            CPPMiner.stop();
        });

        CPPMiner.setHardwareType('cuda')
    },

    async mine() {
        this.miningParameters = {};
        await this.collectMiningParameters();

        if (!this.mining) {
            try {
                // C++ module entry point
                this.mineCoins();
            } catch (e) {
                CPPMiner.log(e.toString())
            }
        }

        //keep on looping!
        setInterval(async() => {
            await this.collectMiningParameters()
        }, COLLECT_MINING_PARAMS_TIMEOUT);
    },

    async collectMiningParameters() {
        try {
            var parameters = await networkInterface.collectMiningParameters(this.miningParameters);

            this.miningParameters.miningDifficulty = parameters.miningDifficulty;
            this.miningParameters.challengeNumber = parameters.challengeNumber;
            this.miningParameters.miningTarget = parameters.miningTarget;
            this.miningParameters.poolEthAddress = parameters.poolEthAddress;

            //give data to the c++ addon
            await this.updateCPUAddonParameters()
        } catch (e) {
            CPPMiner.log(e.toString())
        }
    },

    async updateCPUAddonParameters() {
        if (this.challengeNumber == null || this.challengeNumber != this.miningParameters.challengeNumber) {
            oldChallenge = this.challengeNumber;
            this.challengeNumber = this.miningParameters.challengeNumber

            CPPMiner.setPrefix(this.challengeNumber + this.miningParameters.poolEthAddress.slice(2));
        }

        if (this.miningTarget == null || this.miningTarget != this.miningParameters.miningTarget) {
            this.miningTarget = this.miningParameters.miningTarget

            CPPMiner.setTarget("0x" + this.miningTarget.toString(16, 64));
        }

        if (this.miningDifficulty == null || this.miningDifficulty != this.miningParameters.miningDifficulty) {
            this.miningDifficulty = this.miningParameters.miningDifficulty

            CPPMiner.setDiff(this.miningDifficulty);
            // CPPMiner.setDifficulty( parseInt( this.miningTarget.toString(16, 64).substring(0, 16), 16 ) );
        }
    },

    async mineCoins() {
        const verifyAndSubmit = () => {
            let solution_number = "0x" + CPPMiner.getSolution();
            if (solution_number == "0x" || web3utils.toBN(solution_number).eq(0)) { return; }
            let challenge_number = this.miningParameters.challengeNumber;
            try {
                var digest = web3utils.soliditySha3(challenge_number,
                                                    this.miningParameters.poolEthAddress,
                                                    solution_number);
            } catch(err) {
                CPPMiner.log("Error generating digest:" +
                             "\n chal: " + challenge_number +
                             "\n addr: " + this.miningParameters.poolEthAddress +
                             "\n sol:  " + solution_number);
            }
            let digestBigNumber = web3utils.toBN(digest);
            if (digestBigNumber.lte(this.miningParameters.miningTarget)) {
                this.submitNewMinedBlock(solution_number,
                                         digest,
                                         challenge_number,
                                         this.miningParameters.miningTarget,
                                         this.miningParameters.miningDifficulty)
            } else {
                if (oldChallenge &&
                    solution_number &&
                    web3utils.toBN(web3utils.soliditySha3(oldChallenge,
                                                          this.miningParameters.poolEthAddress,
                                                          solution_number)).lte(this.miningParameters.miningTarget)) {
                    let tempLog = "CPU verification failed: stale solution.";
                    if (jsConfig.submitstale) {
                        tempLog += "\n               Submitting solution anyway.";
                        this.submitNewMinedBlock(solution_number,
                                                 digest,
                                                 challenge_number,
                                                 this.miningParameters.miningTarget,
                                                 this.miningParameters.miningDifficulty)
                    }
                    CPPMiner.log(tempLog);
                } else {
                    failedSolutions++;
                    CPPMiner.log("CPU verification failed:" +
                                 "\n challenge: " + challenge_number +
                                 "\n address:   " + this.miningParameters.poolEthAddress +
                                 "\n solution:  " + solution_number +
                                 "\n digest:    " + digest +
                                 "\n target:    0x" + this.miningParameters.miningTarget.toString(16, 64));
                }
            }
        }

        this.mining = true;

        CPPMiner.run(() => {});

        setInterval(verifyAndSubmit, 500);
    }
}
