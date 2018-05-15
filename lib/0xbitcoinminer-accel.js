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
        networkInterface.init(CPPMiner.getPoolUrl(),
                              CPPMiner.getAddress(),
                              (CPPMiner.getCustomDiff() ? CPPMiner.getDiff() : null),
                              CPPMiner.getTokenName(),
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
        if (solution_number == "0x" || new BN(solution_number).eq(0)) { return; }
        try {
            var digest = `0x${keccak256(new BN(CPPMiner.getPrefix() +
                                               solution_number.slice(2), 16).toBuffer('be'))}`;
        } catch(err) {
            CPPMiner.log(`Error generating digest:
 chal: 0x${CPPMiner.getChallenge()}
 addr: 0x${CPPMiner.getPoolAddress()}
 sol:  ${solution_number}`);
        }
        let digestBigNumber = new BN(digest);
        if (digestBigNumber.lte(new BN(target))) {
            this.submitNewMinedBlock(solution_number,
                                     digest,
                                     `0x${CPPMiner.getChallenge()}`,
                                     target,
                                     CPPMiner.getDiff())
        } else {
            if (oldChallenge &&
                solution_number &&
                new BN(keccak256(CPPMiner.getPreviousChallenge().slice(2),
                                 CPPMiner.getPoolAddress(),
                                 solution_number)).lte(target)) {
                let tempLog = "CPU verification failed: stale solution.";
                if (CPPMiner.getSubmitStale()) {
                    tempLog += "\n               Submitting solution anyway.";
                    this.submitNewMinedBlock(solution_number,
                                             digest,
                                             `0x${CPPMiner.getChallenge()}`,
                                             target,
                                             CPPMiner.getDiff())
                }
                CPPMiner.log(tempLog);
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
