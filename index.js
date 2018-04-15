const Miner = require("./0xbitcoinminer-accel");
const Vault = require("./lib/vault");
const miningLogger = require("./lib/mining-logger");
var prompt = require('prompt');
var pjson = require('./package.json');
var Web3 = require('web3')
var ContractInterface = require("./contracts/DeployedContractInfo")
var NetworkInterface = require("./lib/network-interface");
var PoolInterface = require("./lib/pool-interface");

var web3 = new Web3();

var running = true;

init();

function init() {
    initSignalHandlers();

    prompt.message = null;
    prompt.delimiter = ":";
    prompt.start({ noHandleSIGINT: true });

    if(process.argv.length > 2) {
        handleCommand(process.argv.slice(2).join(' '));
        if(process.argv[2] == 'help') {
            return process.exit();
        } else {
            drawLayout();
        }
    } else {
        drawLayout();
        console.log('Welcome to 0xBitcoin Miner!')
        console.log('Type a command to get started.  Type "help" for a list of commands.')
    }
    return getPrompt();
}

async function getPrompt() {
    var result = await promptForCommand();

    return getPrompt();
}

function sigHandler(signal) {
    process.exit(128 + signal)
}

function initSignalHandlers() {
    process.on('SIGTERM', sigHandler);
    process.on('SIGINT', sigHandler);
    process.on('SIGBREAK', sigHandler);
    process.on('SIGHUP', sigHandler);
    process.on('SIGWINCH', (sig) => {
        process.stdout.write("\x1b[5r\x1b[5;1f");
    });
    process.on('exit', (sig) => {
        process.stdout.write("\x1b[s\x1b[?25h\x1b[r\x1b[u");
    });
}

function drawLayout() {
    process.stdout.write( "\x1b[?25l\x1b[2J\x1b(0" );
    process.stdout.write( "\x1b[1;1flqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqwqqqqqqqqqqqqqqqqqqqqqqqqqqwqqqqqqqqqqqqqqqqqk" );
    process.stdout.write( "\x1b[4;1fmqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqvqqqqqqqqqqqqqqqqqqqqqqqqqqvqqqqqqqqqqqqqqqqqj" );
    process.stdout.write( "\x1b[2;1fx\x1b[2;35fx\x1b[2;62fx\x1b[2;80fx" );
    process.stdout.write( "\x1b[3;1fx\x1b[3;35fx\x1b[3;62fx\x1b[3;80fx" );
    process.stdout.write( "\x1b(B\x1b[2;2fChallenge:" );
    process.stdout.write( "\x1b[3;2fDifficulty:" );
    process.stdout.write( "\x1b[2;37fHashes this round" );
    process.stdout.write( "\x1b[2;63fRound time:" );
    process.stdout.write( "\x1b[3;63fAccount:" );
    process.stdout.write( "\x1b[2;31fMH/s" );
    process.stdout.write( "\x1b[3;31fSols" );
    process.stdout.write( "\x1b[s\x1b[3;29f\x1b[38;5;221m0\x1b[0m\x1b[u" );
    process.stdout.write( "\x1b[1;64fv" + pjson.version );
    process.stdout.write( "\x1b[5r\x1b[5;1f\x1b[?25h" );
}

async function promptForCommand() {
    return new Promise(function (fulfilled, rejected) {
        prompt.get(['command'], async function (err, result) {
            if (err) {
                console.log(err);
                return rejected(err);
            } else {
                var response = await handleCommand(result.command)
                return fulfilled(response);
            }
        });
    });
}

/*
if (process.argv.length <= 2) {
console. log("Please add a subsystem parameter (use 'npm run help' for help)");
process. exit(-1);
}

var subsystem_name =  process.argv[2] ;
var subsystem_command = process.argv[3] ;
var subsystem_option = process.argv[4] ;
*/

async function handleCommand(result) {
    var split_command = result.split(' ');
    //console.log( split_command )

    var subsystem_name = split_command[0];
    var subsystem_command = split_command[1];
    var subsystem_option = split_command[2];

    if (subsystem_name == 'account') {
        await Vault.handleAccountCommand(subsystem_command, subsystem_option)
    }

    if (subsystem_name == 'contract') {
        var unlocked = await Vault.init(web3, miningLogger);
        if (!unlocked) return false;

        await Vault.handleContractCommand(subsystem_command, subsystem_option)
    }

    if (subsystem_name == 'config') {
        var unlocked = await Vault.init(web3, miningLogger);
        if (!unlocked) return false;

        await Vault.handleConfigCommand(subsystem_command, subsystem_option)
    }

    if (subsystem_name == 'mine') {
        Vault.requirePassword(true) //for encryption of private key !

        var unlocked = await Vault.init(web3, miningLogger);
        if (!unlocked) return false;

        NetworkInterface.init(web3, Vault, miningLogger);

        Miner.init(web3, Vault, miningLogger);
        Miner.setNetworkInterface(NetworkInterface);

        Miner.setMiningStyle("solo")

        //us command as option -- for cuda or opencl
        subsystem_option = subsystem_command;
        Miner.mine(subsystem_command, subsystem_option)
    }

    //mining test
    if (subsystem_name == 'test' && subsystem_command == 'mine') {
        Vault.requirePassword(true) //for encryption of private key !

        var infura_provider_url = 'https://ropsten.infura.io/gmXEVo5luMPUGPqg6mhy';
        var ropsten_contract_address = ContractInterface.networks.testnet.contracts._0xbitcointoken.blockchain_address

        Vault.setWeb3ProviderUrl(infura_provider_url);
        Vault.selectContract(ropsten_contract_address);

        web3.setProvider(infura_provider_url)

        var unlocked = await Vault.init(web3, miningLogger);
        if (!unlocked) return false;

        web3.setProvider(infura_provider_url)
        Vault.selectContract(ropsten_contract_address);

        NetworkInterface.init(web3, Vault, miningLogger);

        Miner.init(web3, Vault, miningLogger);
        Miner.setNetworkInterface(NetworkInterface);

        Miner.setMiningStyle("solo")
        Miner.mine(subsystem_command, subsystem_option)
    }

    if (subsystem_name == 'pool') {
        var unlocked = await Vault.init(web3, miningLogger);
        if (!unlocked) return false;

        await PoolInterface.init(web3, subsystem_command, Vault, miningLogger);
        await PoolInterface.handlePoolCommand(subsystem_command, subsystem_option)

        if (subsystem_command == "mine") {
            Miner.init(web3, Vault, miningLogger);
            Miner.setNetworkInterface(PoolInterface);
            Miner.setMiningStyle("pool")
            Miner.mine(subsystem_command, subsystem_option)
        }
    }

    if (subsystem_name == 'exit' || subsystem_name == 'quit') {
        process.exit(0);
    }

    if (subsystem_name == 'help') {
        return printHelp();
    }
}

function printHelp() {
    console.log('--0xBitcoin Miner Help--')
    console.log('Available commands:\n')

    console.log('"account new"            - Create a new mining account')
    console.log('"account list"           - List all mining accounts')
    console.log('"account select 0x####"  - Select the active mining account by address')
    console.log('"account balance"        - List the Ether & token balance of the active account\n')

    console.log('"contract list"          - List the selected token contract to mine')
    console.log('"contract select 0x####" - Select a PoW token contract to mine\n')

    console.log('"config list"            - Show your current configuration')
    console.log('"config gasprice #"      - Set the gasprice used to submit PoW in solo mining')
    //  console.log('"config cpu_threads #"   - Set the number of CPU cores to use for mining ')
    console.log('"config web3provider http://----:####" - Set the web3 provider to submit PoW\n')

    console.log('"pool mine"              - Begin mining into a pool using CPU')
    console.log('"pool mine cuda"         - Begin mining into a pool using CUDA GPU')
    //console.log('"pool mine opencl"       - Begin mining into a pool using OpenCL GPU')
    console.log('"pool list"              - List the selected mining pool')
    console.log('"pool select http://####.com:####" - Select a pool to mine into\n')

    console.log('"test mine"              - Begin solo mining on Ropsten testnet')
    console.log('"mine"                   - Begin solo mining')
    console.log('"mine cuda"              - Begin solo mining using CUDA GPU')
    //console.log('"mine opencl" - Begin mining using OpenCL GPU')
    //  console.log('Encrypted data vault stored at '+ Vault.get0xBitcoinLocalFolderPath())
}
