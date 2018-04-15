const web3utils = require('web3-utils');
const BN = require('bn.js');
const debugLogger = require('./lib/debug-logger')
const miningLogger = require("./lib/mining-logger");
const tokenContractJSON = require('./contracts/_0xBitcoinToken.json');
const CPPMiner = require('./build/Release/hybridminer');

//only load this if selecting 'gpu mine!!!'

var tokenContract;

const PRINT_STATS_TIMEOUT = 5000;
const COLLECT_MINING_PARAMS_TIMEOUT = 4000;
var hardwareType = 'cuda'; //default

module.exports = {
    async init(web3, vault, miningLogger)
    //  async init(web3, subsystem_command, vault, networkInterface, miningLogger)
    {
        process.on('exit', () => {
            miningLogger.print("Process exiting... stopping miner");
            CPPMiner.stop();
        });

        tokenContract = new web3.eth.Contract(tokenContractJSON.abi, vault.getTokenContractAddress());

        this.miningLogger = miningLogger;
        this.vault = vault;
    },

    async mine(subsystem_command, subsystem_option) {
        if (subsystem_option == 'cuda') {
            CPPMiner.setHardwareType('cuda');
        } else if (subsystem_option == 'opencl') {
            CPPMiner.setHardwareType('opencl');
        } else {
            CPPMiner.setHardwareType('cpu');
        }

        if (this.miningStyle == "solo") {
            //if solo mining need a full account
            var eth_account = this.vault.getFullAccount();

            if (eth_account.accountType == "readOnly"
                || !(eth_account.hasOwnProperty('privateKey')
                     && (typeof eth_account.privateKey == 'undefined' || eth_account.privateKey != null))) {
                miningLogger.print("The account", eth_account.address, 'does not have an associated private key. Please select another account or mine to a pool.');
                return;
            }
        } else if (this.miningStyle == "pool") {
            var eth_account = this.vault.getAccount();
        }

        if (eth_account == null || eth_account.address == null) {
            miningLogger.print("Please create a new account with 'account new' before solo mining.")
            return false;
        }

        ///this.mining = true;
        this.minerEthAddress = eth_account.address;

        let miningParameters = {};
        await this.collectMiningParameters(this.minerEthAddress, miningParameters, this.miningStyle);

        this.miningLogger.appendToStandardLog("Begin mining for " + this.minerEthAddress + " @ gasprice " + this.vault.getGasPriceGwei());

        process.stdout.write('\x1b[s\x1b[?25l\x1b[3;72f\x1b[38;5;33m' + this.minerEthAddress.slice(0, 8) + '\x1b[0m\x1b[u\x1b[?25h');

        if (this.miningStyle != "pool") {
            miningLogger.print("Gas price is", this.vault.getGasPriceGwei(), 'gwei');
        }

        //keep on looping!
        setInterval(async() => { await this.collectMiningParameters(this.minerEthAddress, miningParameters, this.miningStyle) }, COLLECT_MINING_PARAMS_TIMEOUT);

        setInterval(() => { this.printMiningStats() }, PRINT_STATS_TIMEOUT);
    },

    mineStuff(miningParameters) {
        if (!this.mining) {
            this.mineCoins(this.web3, miningParameters, this.minerEthAddress);
        }
    },

    setMiningStyle(style) {
        this.miningStyle = style;
    },

    async collectMiningParameters(minerEthAddress, miningParameters, miningStyle) {
        try {
            if (miningStyle === "pool") {
                var parameters = await this.networkInterface.collectMiningParameters(minerEthAddress, miningParameters);
            } else {
                var parameters = await this.networkInterface.collectMiningParameters();
            }

            miningParameters.miningDifficulty = parameters.miningDifficulty;
            miningParameters.challengeNumber = parameters.challengeNumber;
            miningParameters.miningTarget = parameters.miningTarget;
            miningParameters.poolEthAddress = parameters.poolEthAddress;

            //give data to the c++ addon
            await this.updateCPUAddonParameters(miningParameters, miningStyle)
        } catch (e) {
            miningLogger.print(e)
        }
    },

    async updateCPUAddonParameters(miningParameters, miningStyle) {
        let bResume = false;

        if (miningStyle == 'pool' && this.challengeNumber != null) {
            CPPMiner.setChallengeNumber(this.challengeNumber);
            bResume = true;
        }

        if (this.challengeNumber != miningParameters.challengeNumber) {
            this.challengeNumber = miningParameters.challengeNumber

            CPPMiner.setChallengeNumber(this.challengeNumber);
            bResume = true;
            process.stdout.write("\x1b[s\x1b[?25l\x1b[2;13f\x1b[38;5;34m" + this.challengeNumber.substring(2, 10) +
                                 "\x1b[0m\x1b[u\x1b[?25h");
        }

        if (this.miningTarget == null || !this.miningTarget.eq(miningParameters.miningTarget)) {
            this.miningTarget = miningParameters.miningTarget

            CPPMiner.setDifficultyTarget("0x" + this.miningTarget.toString(16, 64));
        }

        if (this.miningDifficulty != miningParameters.miningDifficulty) {
            this.miningDifficulty = miningParameters.miningDifficulty

            process.stdout.write("\x1b[s\x1b[?25l\x1b[3;14f\x1b[38;5;34m" +
                                 this.miningDifficulty.toString().padEnd(7) +
                                 "\x1b[0m\x1b[u\x1b[?25h");
            // CPPMiner.setDifficulty( parseInt( this.miningTarget.toString(16, 64).substring(0, 16), 16 ) );
        }

        if (bResume && !this.mining) {
            try {
                // C++ module entry point
                this.mineStuff(miningParameters);
            } catch (e) {
                miningLogger.print(e)
            }
        }
    },

    //async submitNewMinedBlock(addressFrom, solution_number, digest_bytes, challenge_number)
    submitNewMinedBlock(addressFrom, minerEthAddress, solution_number, digest_bytes, challenge_number, target, difficulty) {
        this.networkInterface.queueMiningSolution(addressFrom, minerEthAddress, solution_number, digest_bytes, challenge_number, target, difficulty)
    },

    // contractData , -> miningParameters
    mineCoins(web3, miningParameters, minerEthAddress) {
        let target = miningParameters.miningTarget;
        let difficulty = miningParameters.miningDifficulty;

        let addressFrom;

        if (this.miningStyle == "pool") {
            addressFrom = miningParameters.poolEthAddress;
        } else {
            addressFrom = minerEthAddress;
        }

        CPPMiner.setMinerAddress(addressFrom);

        const verifyAndSubmit = () => {
            let solution_number = "0x" + CPPMiner.getSolution();
            if(solution_number == "0x" || web3utils.toBN(solution_number).eq(0)) { return; }
            let challenge_number = miningParameters.challengeNumber;
            let digest = web3utils.soliditySha3(challenge_number,
                                                addressFrom,
                                                solution_number);
            let digestBigNumber = web3utils.toBN(digest);
            if (digestBigNumber.lte(miningParameters.miningTarget)) {
                //  this.submitNewMinedBlock(minerEthAddress, solution_number, digest, challenge_number);
                this.submitNewMinedBlock(addressFrom, minerEthAddress, solution_number,
                                         digest, challenge_number, target, difficulty)
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

        debugLogger.log('MINING:', this.mining)

        CPPMiner.run((err, sol) => {
            if (sol) {
                try {
                    verifyAndSubmit(sol);
                } catch (e) {
                    miningLogger.print(e)
                }
            }
            this.mining = false;

            debugLogger.log('MINING:', this.mining)
        });
    },

    setHardwareType(type) {
        hardwareType = type;
        miningLogger.print("Set hardware type:", type)
    },

    setNetworkInterface(netInterface) {
        this.networkInterface = netInterface;
    },

    printMiningStats() {
        var hashes = CPPMiner.hashes();
        //  miningLogger.print('hashes:', hashes )
        //miningLogger.print('Hash rate: ' + parseInt(hashes / PRINT_STATS_TIMEOUT) + " kH/s");
    }
}
