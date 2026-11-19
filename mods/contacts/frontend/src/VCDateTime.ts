/**
 * vCard DateTime
 */
export class VCDateTime {
    year?: string;
    month?: string;
    day?: string;
    hour?: string;
    min?: string;
    sec?: string;
    zone?: string;

    static parse(str: string, bareTime = false) {
        // Start with null datetime
        const ret = new VCDateTime();

        // TODO replace this with something more performant
        const dateRegex =
            /(?:^(?<year>[0-9]{4})(?<month>[01][0-9])(?<day>[0-3][0-9])$)/.source
            + /|(?:^(?<year>(?:\+[0-9]+)?[0-9]{4})-(?<month>[01][0-9])-(?<day>[0-3][0-9])$)/.source
            + /|(?:^--(?<month>[01][0-9])(?<day>[0-3][0-9])$)/.source
            + /|(?:^---(?<day>[0-3][0-9])$)/.source
            + /|(?:^(?<year>[0-9]{4})-(?<month>[01][0-9])$)/.source
            + /|(?:^(?<year>[0-9]{4})(?<month>[01][0-9])$)/.source
            + /|(?:^(?<year>[0-9]{4})$)/.source;
        const timeRegex =
            /(?:^(?<hour>[0-2][0-9])(?<min>[0-6][0-9])(?<sec>[0-6][0-9])(?<zone>Z|(?:[+\-][0-2][0-9](?:[0-2][0-9])?))?$)/.source
            + /|(?:^(?<hour>[0-2][0-9]):(?<min>[0-6][0-9]):(?<sec>[0-6][0-9])(?<zone>Z|(?:[+\-][0-2][0-9](?::[0-2][0-9])?))?$)/.source
            + /|(?:^(?<hour>[0-2][0-9])(?<min>[0-6][0-9])(?<sec>[0-6][0-9])$)/.source
            + /|(?:-(?<min>[0-6][0-9])(?<sec>[0-6][0-9])$)/.source
            + /|(?:--(?<sec>[0-6][0-9])$)/.source
            + /|(?:^(?<hour>[0-2][0-9])(?<min>[0-6][0-9])$)/.source
            + /|(?:^(?<hour>[0-2][0-9])$)/.source;

        if (bareTime) {
            const m = str.match(timeRegex);
            if (!m)
                return null;
            ret.hour = m.groups.hour;
            ret.min = m.groups.min;
            ret.sec = m.groups.sec;
            ret.zone = m.groups.zone;
            return ret;
        }

        const [dateStr, timeStr] = str.split('T');

        if (dateStr) {
            const m = dateStr.match(dateRegex);
            if (!m) {
                console.log('invalid date: ', dateStr);
                // return null;
            }
            ret.year = m.groups.year;
            ret.month = m.groups.month;
            ret.day = m.groups.day;
        }
        if (timeStr) {
            const m = timeStr.match(timeRegex);
            if (!m) {
                console.log('invalid time2', timeStr);
                // return null;
            }
            ret.hour = m.groups.hour;
            ret.min = m.groups.min;
            ret.sec = m.groups.sec;
            ret.zone = m.groups.zone;
        }

        return ret;
    }

    static now(): VCDateTime {
        const now = new Date().toISOString();
        const ret = new VCDateTime();
        const [d, t] = now.split('T');
        [ret.year, ret.month, ret.day] = d.split('-');
        [ret.hour, ret.min, ret.sec] = t.split(':')
            .map(v => parseInt(v).toString());
        return ret;
    }

    toUserString() {
        const monthNames = [
            '', // There is no month zero
            'January',
            'February',
            'March',
            'April',
            'May',
            'June',
            'August',
            'September',
            'October',
            'November',
            'December'
        ];

        const monthName = idx => {
            const d = new Date();
            d.setDate(1);
            d.setMonth(idx);
            return d.toLocaleString(undefined, { month: 'long' });
        }

        let ret = '';
        if (this.year && this.month && this.day) {
            ret += new Date(
                parseInt(this.year),
                parseInt(this.month),
                parseInt(this.day)
            ).toLocaleDateString() + ' ';
        } else {
            if (!this.year)
                ret += 'Every ';
            if (this.month)
                ret += monthName(parseInt(this.month));
            if (this.day) {
                ret += " ";
                const iday = parseInt(this.day);
                ret += iday;
                // const ord = iday % 10 === 1
                //     ? 'st'
                //     : iday % 10 === 2
                //         ? 'nd'
                //         : 'th';
                // ret += ord;
                if (this.year)
                    ret += ', ';
            }
            if (this.year)
                ret += this.year + ' ';
        }

        // Add time
        if (!(this.hour || this.min || this.sec)) {
            return ret;
        }
        if (this.hour && this.min && this.sec) {
            const d = new Date();
            d.setHours(parseInt(this.hour));
            d.setMinutes(parseInt(this.min));
            d.setSeconds(parseInt(this.sec));
            ret += d.toLocaleTimeString();
        } else {
            ret += "at ";
            ret += this.hour || '**';
            ret += ':';
            ret += this.min || '**';
            if (this.sec)
                ret += ':' + this.sec;
        }

        // Add zone info
        if (this.zone) {
            ret += ' UTC';
            if (this.zone !== 'Z')
                ret += this.zone;
        }
        return ret;
    }

    toVCString() {
        let ret = '';
        if (this.year || this.month || this.day) {
            ret += this.year || '--';
            ret += this.month || (this.day ? '-' : '');
            if (this.day)
                ret += this.day;
        }
        if (this.hour || this.min || this.sec) {
            ret += 'T';
            ret += this.hour || '-';
            ret += this.min || (this.sec ? '-' : '');
            if (this.sec)
                ret += this.sec;
        }
        if (this.zone)
            ret += this.zone;

        return ret;
    }
}