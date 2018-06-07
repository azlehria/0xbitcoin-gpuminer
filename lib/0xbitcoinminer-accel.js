'use strict';

const BN = require('bn.js');
const CPPMiner = require('../build/Release/hybridminer.node');
const networkInterface = require("./pool-interface");

const COLLECT_MINING_PARAMS_TIMEOUT = 4000;

module.exports = {
    init()
    {
        this.submitNewMinedBlock = networkInterface.queueMiningSolution;
        networkInterface.init(CPPMiner.getPoolUrl(),
                              CPPMiner.getAddress(),
                              (CPPMiner.getCustomDiff() ? CPPMiner.getDiff() : null),
                              CPPMiner.getMaximumTarget(),
                              CPPMiner.resetHashCounter,
                              CPPMiner.incSolCount,
                              CPPMiner.log);

        this.collectMiningParameters();
    },

    async mine() {
        if (!this.mining) {
            try {
                this.mining = true;

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
            var parameters = await networkInterface.collectMiningParameters();

            CPPMiner.setPrefix(parameters.challengeNumber + parameters.poolEthAddress.slice(2));

            if (!CPPMiner.getCustomDiff()) {
                CPPMiner.setDiff(parameters.miningDifficulty);
            }
        } catch (e) {
            CPPMiner.log(e.toString())
        }
    },

    async verifyAndSubmit() {
        let solution_number = `0x${CPPMiner.getSolution()}`;
        let target = CPPMiner.getTarget();
        if (solution_number == "0x" || new BN(solution_number,16).eq(0)) { return; }
        try {
            var digest = `0x${CPPMiner.keccak256(CPPMiner.getPrefix() + solution_number.slice(2))}`
        } catch(err) {
            CPPMiner.log(`Error generating digest:
 chal: 0x${CPPMiner.getChallenge()}
 addr: 0x${CPPMiner.getPoolAddress()}
 sol:  ${solution_number}`);
        }
        let digestBigNumber = new BN(digest,16);
        if (digestBigNumber.lte(new BN(target,16))) {
            this.submitNewMinedBlock(solution_number,
                                     digest,
                                     `0x${CPPMiner.getChallenge()}`,
                                     target,
                                     CPPMiner.getDiff())
        } else {
            if (solution_number &&
                new BN(CPPMiner.keccak256(CPPMiner.getPreviousChallenge().slice(2),
                                          CPPMiner.getPoolAddress(),
                                          solution_number.slice(2)), 16).lte(target)) {
                CPPMiner.log("CPU verification failed: stale solution.");
            } else {
                CPPMiner.log(`CPU verification failed:
 challenge: 0x${CPPMiner.getChallenge()}
 address:   0x${CPPMiner.getPoolAddress()}
 solution:  ${solution_number}
 digest:    ${digest}
 target:    ${target}`);
            }
        }
    }
}
