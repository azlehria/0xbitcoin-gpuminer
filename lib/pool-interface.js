const MINING_POOL_URL = 'http://tokenminingpool.com:8586';

const web3Utils = require('web3-utils')
const Tx = require('ethereumjs-tx')
const Vault = require("./vault");
const miningLogger = require("./mining-logger");
const jayson = require('jayson');
const tokenContractJSON = require('../contracts/_0xBitcoinToken.json');
var busySendingSolution = false;
var queuedMiningSolutions = [];
var lastSubmittedMiningSolutionChallengeNumber;

module.exports = {
    init(web3, subsystem_command, vault, miningLogger) {
        this.web3 = web3;
        this.tokenContract = new web3.eth.Contract(tokenContractJSON.abi, vault.getTokenContractAddress())
        this.miningLogger = miningLogger;
        this.vault = vault;
        this.solutionsSubmitted = 0;
        busySendingSolution = false;

        if (this.vault.getMiningPool() == null) {
            this.vault.selectMiningPool(MINING_POOL_URL)
        }

        this.jsonrpcClient = jayson.client.http(
            this.vault.getMiningPool()
        );

        setInterval(async () => { await this.sendMiningSolutions() }, 500)
    },

    async handlePoolCommand(subsystem_command, subsystem_option) {
        if (subsystem_command === 'select') {
            this.vault.selectMiningPool(subsystem_option); //pool url
            await this.vault.saveVaultData();
        }

        if (subsystem_command === 'show' || subsystem_command === 'list') {
            miningLogger.print('Selected mining pool:', this.vault.getMiningPool())
        }
    },

    /*
        async checkMiningSolution(addressFrom,solution_number,challenge_digest,challenge_number,target,callback){
          this.tokenContract.methods.checkMintSolution(solution_number,challenge_digest, challenge_number, target).call(callback)
        },
    */

    //the miner will ask for this info to help find solutions !!
    hasReceivedPoolConfig() {
        return this.receivedPoolConfig;
    },

    getPoolEthAddress() {
        return this.poolEthAddress;
    },

    getMinimumShareDifficulty() {
        return this.poolMinimumShareDifficulty;
    },

    //JSONRPC interface to the pool
    async collectMiningParameters(minerEthAddress, previousMiningParameters) {
        // create a client
        const args = []
        let rpcRequests = [
            this.jsonrpcClient.request('getPoolEthAddress', args),
            this.jsonrpcClient.request('getChallengeNumber', args),
            this.jsonrpcClient.request('getMinimumShareTarget', [minerEthAddress]),
            this.jsonrpcClient.request('getMinimumShareDifficulty', [minerEthAddress])
        ];
        let rpcResponses = await new Promise((fulfilled, rejected) => {
            this.jsonrpcClient.request(rpcRequests, (err, responses) => {
                if (err) { rejected(err); return; }
                if (typeof responses == 'undefined') { rejected(responses); return; }

                fulfilled(responses)
            });
        });

        const selectRpcResponse = (rpcRequest) => {
            for(var i = 0; i < rpcResponses.length; ++i)
            {
                if( rpcResponses[i].id == rpcRequest.id )
                    return rpcResponses[i].result;
                else if( rpcResponses[i].id == rpcRequest.id )
                    return rpcResponses[i].result;
                else if( rpcResponses[i].id == rpcRequest.id )
                    return rpcResponses[i].result;
                else if( rpcResponses[i].id == rpcRequest.id )
                    return rpcResponses[i].result;
            }
        }

        // if one of these displays as '0' then we have a problem
        // but at least then we'll _know about it_
        var poolEthAddress = selectRpcResponse(rpcRequests[0]) || previousMiningParameters.poolEthAddress || '0x0';
        var poolChallengeNumber = selectRpcResponse(rpcRequests[1]) || previousMiningParameters.poolChallengeNumber || '0x0';
        var poolMinimumShareTarget = selectRpcResponse(rpcRequests[2]) || previousMiningParameters.poolMinimumShareTarget || '0x0';
        var poolMinimumShareDifficulty = selectRpcResponse(rpcRequests[3]) || previousMiningParameters.poolMinimumShareDifficulty || '0x0';

        this.receivedPoolConfig = true;

        return {
            miningDifficulty: poolMinimumShareDifficulty,
            challengeNumber: poolChallengeNumber,
            miningTarget: web3Utils.toBN(poolMinimumShareTarget),
            poolEthAddress: poolEthAddress
        };
    },

    async queryMiningVars(minerEthAddress, previousMiningParameters) {
        const args = {
            'clientEthAddress': minerEthAddress,
            // 'clientWorkerName': minerWorkerName,
            'poolEthAddress': poolEthAddress,
            'challengeNumber': poolChallengeNumber,
            'shareTarget': poolMinimumShareTarget,
            'shareDifficulty': poolMinimumShareDifficulty
        };

        let rpcResponse = await new Promise((fulfilled, rejected) => {
            this.jsonrpcClient.request('queryMiningVars', args, (err, response) => {
                if (err) { rejected(err); return; }
                if (typeof response == 'undefined') { rejected(response); return; }

                fulfilled(responses)
            });
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
        var poolMinimumShareDifficulty = rpcResponse.result.shareDifficulty || previousMiningParameters.poolMinimumShareDifficulty || '0x0';

        this.receivedPoolConfig = true;

        return {
            miningDifficulty: poolMinimumShareDifficulty,
            challengeNumber: poolChallengeNumber,
            miningTarget: web3Utils.toBN(poolMinimumShareTarget),
            poolEthAddress: poolEthAddress
        };
    },

    async sendMiningSolutions() {
        const incrementSolutionCount = async(number) => {
            this.solutionsSubmitted += number;
            process.stdout.write("\x1b[s\x1b[?25l\x1b[3;22f\x1b[38;5;221m" +
                                 this.solutionsSubmitted.toString().padStart(8) +
                                 "\x1b[0m\x1b[u\x1b[?25h");
        }

        if (busySendingSolution == false && queuedMiningSolutions.length > 0) {
            let rpcRequests = [];
            while (queuedMiningSolutions.length > 0) {
                let nextSolution = queuedMiningSolutions.pop();

                rpcRequests.push(this.jsonrpcClient.request('submitShare', [nextSolution.solution_number,
                                                                            nextSolution.minerEthAddress,
                                                                            nextSolution.challenge_digest,
                                                                            nextSolution.difficulty,
                                                                            nextSolution.challenge_number]));

                this.miningLogger.appendToStandardLog("Sending queued solution", nextSolution.toString())

                //try {
                //    await this.submitMiningSolution(nextSolution.addressFrom, nextSolution.minerEthAddress,
                //                                    nextSolution.solution_number, nextSolution.challenge_number,
                //                                    nextSolution.challenge_digest, nextSolution.target,
                //                                    nextSolution.difficulty);
                //} catch (e) {
                //    this.miningLogger.appendToErrorLog(e)
                //    miningLogger.print(e);
                //}
            } // while

            await new Promise((fulfilled, rejected) => {
                this.jsonrpcClient.request(rpcRequests, (err, response) => {
                    if (err) { rejected(err); return }
                    if (typeof response == 'undefined') { rejected(response); return; }
                    fulfilled(response.result)
                });
            }).then((res) => {
                incrementSolutionCount(rpcRequests.length);
            },(err) => {
                throw new Error(err);
            }).catch((e) => {
                this.miningLogger.appendToErrorLog(e)
                miningLogger.print(e);
            });
        }
    },

    async queueMiningSolution(addressFrom, minerEthAddress, solution_number, challenge_digest, challenge_number, target, difficulty) {
        queuedMiningSolutions.push({
            addressFrom: addressFrom, //the pool in the pools case,  the miner if solo mining
            minerEthAddress: minerEthAddress, // ALWAYS miner eth address
            solution_number: solution_number,
            challenge_digest: challenge_digest,
            challenge_number: challenge_number,
            target: target,
            difficulty: difficulty
        });
    },

    async submitMiningSolution(addressFrom, minerEthAddress, solution_number, challenge_number, challenge_digest, target, difficulty) {
        this.miningLogger.appendToStandardLog("Submitting Solution " + challenge_digest)

        let args = []
        args[0] = solution_number;
        args[1] = minerEthAddress;
        args[2] = challenge_digest;
        args[3] = difficulty;
        args[4] = challenge_number;

        //add me
        //args[5] = worker_name;
        //args[6] = hashrate;

        return new Promise((fulfilled, rejected) => {
            this.jsonrpcClient.request('submitShare', args, (err, response) => {
                if (err) { rejected(err); return }
                if (typeof response == 'undefined') { rejected(response); return; }
                fulfilled(response.result)
            });
        });
    }
}
