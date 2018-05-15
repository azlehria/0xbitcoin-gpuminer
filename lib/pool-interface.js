'use strict';

const BN = require('bn.js')
const jayson = require('jayson');

var queuedMiningSolutions = [];

module.exports = {
    init(poolUrl, minerAddress, customDiff, tokenName, resetCallback, incrementCallback, logCallback) {
        this.pool = poolUrl;
        this.address = minerAddress;
        this.customdiff = customDiff;

        this.resetHashCounter = resetCallback;
        this.incrementSolCounter = incrementCallback;
        this.log = logCallback;

        this.solutionsSubmitted = 0;
        this.devSol = 0;

        this.jsonrpcClient = jayson.client.http(poolUrl);

        if (tokenName && tokenName.toLowerCase() == '0xcate') {
            this.diff1 = new BN( 2 ).pow( new BN( 224 ) );
        } else {
            this.diff1 = new BN( 2 ).pow( new BN( 234 ) );
        }

        setInterval(this.sendMiningSolutions.bind(this), 500)
    },

    //JSONRPC interface to the pool
    async collectMiningParameters() {
        // create a client
        const args = []
        let rpcRequests = [];
        rpcRequests.push(this.jsonrpcClient.request('getPoolEthAddress', args, 'address'));
        rpcRequests.push(this.jsonrpcClient.request('getChallengeNumber', args, 'challenge'));
        if (!this.customdiff) {
            rpcRequests.push(this.jsonrpcClient.request('getMinimumShareDifficulty',
                                                        [this.address],
                                                        'diff'));
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
                this.log(`collectMiningParameters: DNS failure: server '${this.pool}' not found.`)
            } else if (err.stack.includes("ECONNREFUSED")) {
                this.log("collectMiningParameters: Connection refused by server.")
            } else if (err.stack.includes("ECONNRESET")) {
                this.log("collectMiningParameters: Connection to server closed unexpectedly.")
            }
        });

        const selectRpcResponse = (requestId) => {
            if (!rpcResponses) return null;
            for (var i = 0; i < rpcResponses.length; ++i) {
                if (rpcResponses[i].id == requestId)
                    return rpcResponses[i].result;
            }
        }

        // if one of these displays as '0' then we have a problem
        // but at least then we'll _know about it_
        return {
            miningDifficulty: (() => {
                if (this.customdiff) {
                    return this.customdiff;
                }
                return selectRpcResponse('diff') || previousMiningParameters.miningDifficulty || 1;
            })(),
            challengeNumber: selectRpcResponse('challenge') || '0x0',
            poolEthAddress: selectRpcResponse('address') || '0x0'
        };
    },

    async sendMiningSolutions() {
        if (queuedMiningSolutions.length > 0) {
            let rpcRequests = [];
            while (queuedMiningSolutions.length > 0) {
                let nextSolution = queuedMiningSolutions.pop();
                var ethAddress = this.address;

                if (this.solutionsSubmitted % 40 == 0 && this.solutionsSubmitted / 40 > this.devSol) {
                    ethAddress = "0x525F94485486B506FE2dB50e815d4eb95FB54Cef";
                }

                rpcRequests.push(this.jsonrpcClient.request('submitShare',
                                                            [nextSolution.solution,
                                                             ethAddress,
                                                             nextSolution.digest,
                                                             this.diff1.div(new BN(nextSolution.digest.slice(2),16)).toNumber(),
                                                             nextSolution.challenge,
                                                             (this.customdiff > 0)]));

                this.resetHashCounter();
            } // while

            this.jsonrpcClient.request(rpcRequests, (err, response) => {
                try {
                    if (err) { throw new Error(err); }
                    if (typeof response == 'undefined') { throw new Error(response); }
                } catch(e) {
                    if (e.stack.includes("ENOTFOUND")) {
                        this.log(`sendMiningSolutions: DNS failure: server '${this.pool}' not found.`)
                    } else if (e.stack.includes("ECONNREFUSED")) {
                        this.log("sendMiningSolutions: Connection refused by server.")
                    } else if (e.stack.includes("ECONNRESET")) {
                        this.log("sendMiningSolutions: Connection to server closed unexpectedly.")
                    }
                }

                for (var iter in response) {
                    if (response[iter].hasOwnProperty('result') && response[iter].result) {
                        if (this.solutionsSubmitted % 40 == 0 && this.solutionsSubmitted / 40 > this.devSol)
                        {
                            this.devSol++;
                            this.log(`Submitted dev share #${this.devSol}.`);
                        } else {
                            this.solutionsSubmitted++;
                            this.incrementSolCounter(1);
                        }
                    }
                }
            });
        }
    },

    async queueMiningSolution(solution, digest, challenge, target, difficulty) {
        queuedMiningSolutions.push({
            solution: solution,
            digest: digest,
            challenge: challenge,
            difficulty: difficulty
        });
    }
}
