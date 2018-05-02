'use strict';

module.exports = {
    init(vault) {
    },

    async print() {
        var date = new Date();

        if (oldWindows) {
            var stamp = "\r[" + date.getHours().toString().padStart(2, "0") + ":" +
                        date.getMinutes().toString().padStart(2, "0") + ":" +
                        date.getSeconds().toString().padStart(2, "0") + "." +
                        date.getMilliseconds().toString().padStart(3, "0") + "]";
            return console.log(stamp, ...arguments);
        }

        var stamp = "\x1b[?25l\x1b[2K\x1b[1G\x1b[38;5;249m[" +
                    date.getHours().toString().padStart(2, "0") + ":" +
                    date.getMinutes().toString().padStart(2, "0") + ":" +
                    date.getSeconds().toString().padStart(2, "0") + "." +
                    date.getMilliseconds().toString().padStart(3, "0") +
                    "]\x1b[0m";
        console.log(stamp, ...arguments);
        return process.stdout.write('\x1b[?25h');
    }
}
