'use strict';

const BN = require('bn.js');
const keccak256 = require('js-sha3').keccak_256;
const CPPMiner = require('../build/Release/hybridminer.node');
const networkInterface = require("./pool-interface");

const COLLECT_MINING_PARAMS_TIMEOUT = 4000;
var oldChallenge;

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
        if (!this.mining) {
            try {
                this.mining = true;

                await this.collectMiningParameters();

                // C++ module entry point
                CPPMiner.run(() => {});

                setInterval(this.verifyAndSubmit.bind(this), 100);
                setInterval(this.collectMiningParameters.bind(this), COLLECT_MINING_PARAMS_TIMEOUT);
            } catch (e) {
                CPPMiner.log(e.toString())
            }
        }
    },

    async collectMiningParameters() {
        try {
            var parameters = await networkInterface.collectMiningParameters(this.miningParameters);

            this.poolEthAddress = parameters.poolEthAddress;

            if (!this.challengeNumber || this.challengeNumber != parameters.challengeNumber) {
                oldChallenge = this.challengeNumber;
                this.challengeNumber = parameters.challengeNumber

                CPPMiner.setPrefix(this.challengeNumber + parameters.poolEthAddress.slice(2));
            }

            if (!this.miningTarget || this.miningTarget != parameters.miningTarget) {
                this.miningTarget = parameters.miningTarget

                CPPMiner.setTarget(`0x${this.miningTarget.toString(16, 64)}`);
            }

            if (!this.miningDifficulty || this.miningDifficulty != parameters.miningDifficulty) {
                this.miningDifficulty = parameters.miningDifficulty

                CPPMiner.setDiff(this.miningDifficulty);
            }
        } catch (e) {
            CPPMiner.log(e.toString())
        }
    },

    async verifyAndSubmit() {
        let solution_number = "0x" + CPPMiner.getSolution();
        if (solution_number == "0x" || new BN(solution_number).eq(0)) { return; }
        try {
            var digest = `0x${keccak256(new BN(this.challengeNumber.slice(2) +
                                               this.poolEthAddress.slice(2) +
                                               solution_number.slice(2), 16).toBuffer('be'))}`;
        } catch(err) {
            CPPMiner.log(`Error generating digest:
 chal: ${this.challengeNumber}
 addr: ${this.poolEthAddress}
 sol:  ${solution_number}`);
        }
        let digestBigNumber = new BN(digest);
        if (digestBigNumber.lte(this.miningTarget)) {
            this.submitNewMinedBlock(solution_number,
                                     digest,
                                     this.challengeNumber,
                                     this.miningTarget,
                                     this.miningDifficulty)
        } else {
            if (oldChallenge &&
                solution_number &&
                new BN(keccak256(oldChallenge.slice(2),
                                 this.poolEthAddress,
                                 solution_number)).lte(this.miningTarget)) {
                let tempLog = "CPU verification failed: stale solution.";
                if (jsConfig.submitstale) {
                    tempLog += "\n               Submitting solution anyway.";
                    this.submitNewMinedBlock(solution_number,
                                             digest,
                                             this.challengeNumber,
                                             this.miningTarget,
                                             this.miningDifficulty)
                }
                CPPMiner.log(tempLog);
            } else {
                CPPMiner.log(`CPU verification failed:
 challenge: ${this.challengeNumber}
 address:   ${this.poolEthAddress}
 solution:  ${solution_number}
 digest:    ${digest}
 target:    0x${this.miningTarget.toString(16, 64)}`);
            }
        }
    }
}
