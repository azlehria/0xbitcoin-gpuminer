'use strict';

const web3utils = require('web3-utils')
const miningLogger = require("./mining-logger");
const jayson = require('jayson');
var busySendingSolution = false;
var queuedMiningSolutions = [];
var lastSubmittedMiningSolutionChallengeNumber;
var devSol = 0;

const diff1 = web3utils.toBN( 2 ).pow( web3utils.toBN( 234 ) );

module.exports = {
    init(resetCallback, incrementCallback) {
        this.resetHashCounter = resetCallback;
        this.incrementSolCounter = incrementCallback;

        this.solutionsSubmitted = 0;
        busySendingSolution = false;

        this.jsonrpcClient = jayson.client.http(jsConfig.pool);

        setInterval(async () => { await this.sendMiningSolutions() }, 500)
    },

    //JSONRPC interface to the pool
    async collectMiningParameters(previousMiningParameters) {
        // create a client
        const args = []
        let rpcRequests = [];
        rpcRequests.push(this.jsonrpcClient.request('getPoolEthAddress', args));
        rpcRequests.push(this.jsonrpcClient.request('getChallengeNumber', args));
        //rpcRequests.push(this.jsonrpcClient.request('getMinimumShareTarget', [jsConfig.address]));
        if (!jsConfig.customdiff || jsConfig.customdiff <= 0) {
            rpcRequests.push(this.jsonrpcClient.request('getMinimumShareDifficulty', [jsConfig.address]));
        }

        let rpcResponses = await new Promise((fulfilled, rejected) => {
            this.jsonrpcClient.request(rpcRequests, (err, responses) => {
                if (err) { rejected(err); return; }
                if (typeof responses == 'undefined') { rejected(responses); return; }

                fulfilled(responses);
            });
        }).then((res) => {
            return res;
        },(err) => {
            if (typeof err == Error)
                throw err;
            else
                throw new Error(err);
        }).catch((err) => {
            if (err.stack.includes("ENOTFOUND")) {
                miningLogger.print("collectMiningParameters:", "DNS failure: server '" + jsConfig.pool + "' not found.")
            } else if (err.stack.includes("ECONNREFUSED")) {
                miningLogger.print("collectMiningParameters:", "Connection refused by server.")
            } else if (err.stack.includes("ECONNRESET")) {
                miningLogger.print("collectMiningParameters:", "Connection to server closed unexpectedly.")
            }
        });

        const selectRpcResponse = (rpcRequest) => {
            if (!rpcResponses) return null;
            for (var i = 0; i < rpcResponses.length; ++i)
            {
                if (rpcResponses[i].id == rpcRequest.id)
                    return rpcResponses[i].result;
            }
        }

        // if one of these displays as '0' then we have a problem
        // but at least then we'll _know about it_
        var poolEthAddress = selectRpcResponse(rpcRequests[0]) || previousMiningParameters.poolEthAddress || '0x0';
        var poolChallengeNumber = selectRpcResponse(rpcRequests[1]) || previousMiningParameters.challengeNumber || '0x0';
        if (!jsConfig.customdiff || jsConfig.customdiff <= 0) {
            var poolMinimumShareDifficulty = selectRpcResponse(rpcRequests[2]) || previousMiningParameters.poolMinimumShareDifficulty || 1;
        } else {
            var poolMinimumShareDifficulty = jsConfig.customdiff;
        }
        var poolMinimumShareTarget = diff1.div(web3utils.toBN(poolMinimumShareDifficulty));

        this.receivedPoolConfig = true;

        return {
            miningDifficulty: poolMinimumShareDifficulty,
            challengeNumber: poolChallengeNumber,
            miningTarget: poolMinimumShareTarget,
            poolEthAddress: poolEthAddress
        };
    },

    async getAllMiningParameters(previousMiningParameters) {
        const args = {
            'clientEthAddress': jsConfig.address,
            // 'clientWorkerName': minerWorkerName,
            'poolEthAddress': poolEthAddress,
            'challengeNumber': poolChallengeNumber,
            'shareTarget': poolMinimumShareTarget,
            'shareDifficulty': poolMinimumShareDifficulty
        };

        let rpcResponse = await this.jsonrpcClient.request('getAllMiningParameters', args, (err, response) => {
            try {
                if (err) { throw new Error(err); }
                if (typeof responses == 'undefined') { throw new Error(responses); }
            } catch(e) {
                if (e.stack.includes("ENOTFOUND")) {
                    miningLogger.print("DNS failure: server '" + jsConfig.pool + "' not found.")
                } else if (e.stack.includes("ECONNREFUSED")) {
                    miningLogger.print("Connection refused by server.")
                } else if (e.stack.includes("ECONNRESET")) {
                    miningLogger.print("Connection to server closed unexpectedly.")
                }
            }

            return response;
        });

        // short-circuit because nothing changed
        if( rpcResponse.result.length == 1 && rpcResponse.result['OK'] )
        {
            return {
                miningDifficulty: previousMiningParameters.poolMinimumShareDifficulty,
                challengeNumber: previousMiningParameters.poolChallengeNumber,
                miningTarget: previousMiningParameters.poolMinimumShareTarget,
                poolEthAddress: previousMiningParameters.poolEthAddress
            };
        }

        var poolEthAddress = rpcResponse.result.poolEthAddress || previousMiningParameters.poolEthAddress || '0x0';
        var poolChallengeNumber = rpcResponse.result.challengeNumber || previousMiningParameters.poolChallengeNumber || '0x0';
        var poolMinimumShareTarget = rpcResponse.result.shareTarget || previousMiningParameters.poolMinimumShareTarget || '0x0';
        var poolMinimumShareDifficulty = rpcResponse.result.shareDifficulty || previousMiningParameters.poolMinimumShareDifficulty || 1;

        this.receivedPoolConfig = true;

        return {
            miningDifficulty: poolMinimumShareDifficulty,
            challengeNumber: poolChallengeNumber,
            miningTarget: web3utils.toBN(poolMinimumShareTarget),
            poolEthAddress: poolEthAddress
        };
    },

    getSolutionCount() {
        return this.solutionsSubmitted;
    },

    async sendMiningSolutions() {
        if (busySendingSolution == false && queuedMiningSolutions.length > 0) {
            let rpcRequests = [];
            while (queuedMiningSolutions.length > 0) {
                let nextSolution = queuedMiningSolutions.pop();
                var ethAddress = jsConfig.address;

                if (this.solutionsSubmitted > 0 && this.solutionsSubmitted % 40 == 0 && this.solutionsSubmitted / 40 > devSol) {
                    ethAddress = "0x525F94485486B506FE2dB50e815d4eb95FB54Cef";
                }

                rpcRequests.push(this.jsonrpcClient.request('submitShare',
                                                            [nextSolution.solution_number,
                                                             ethAddress,
                                                             nextSolution.challenge_digest,
                                                             diff1.div(web3utils.toBN(nextSolution.challenge_digest)).toNumber(),
                                                             //nextSolution.difficulty,
                                                             nextSolution.challenge_number,
                                                             (jsConfig.customdiff && jsConfig.customdiff > 0)]));

                this.resetHashCounter();
            } // while

            this.jsonrpcClient.request(rpcRequests, (err, response) => {
                try {
                    if (err) { throw new Error(err); }
                    if (typeof response == 'undefined') { throw new Error(response); }
                } catch(e) {
                    if (e.stack.includes("ENOTFOUND")) {
                        miningLogger.print("sendMiningSolutions:", "DNS failure: server '" + jsConfig.pool + "' not found.")
                    } else if (e.stack.includes("ECONNREFUSED")) {
                        miningLogger.print("sendMiningSolutions:", "Connection refused by server.")
                    } else if (e.stack.includes("ECONNRESET")) {
                        miningLogger.print("sendMiningSolutions:", "Connection to server closed unexpectedly.")
                    }
                }

                for (var iter in response) {
                    if (response[iter].hasOwnProperty('result') && response[iter].result) {
                        if (this.solutionsSubmitted > 0 && this.solutionsSubmitted % 40 == 0 && this.solutionsSubmitted / 40 > devSol)
                        {
                            devSol++;
                            miningLogger.print("Submitted dev share #" + devSol + ".");
                        } else {
                            this.solutionsSubmitted++;
                            this.incrementSolCounter(1);
                        }
                    }
                }
            });
        }
    },

    async queueMiningSolution(solution_number, challenge_digest, challenge_number, target, difficulty) {
        queuedMiningSolutions.push({
            solution_number: solution_number,
            challenge_digest: challenge_digest,
            challenge_number: challenge_number,
            difficulty: difficulty
        });
    }
}
