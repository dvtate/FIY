//
// vCard field
//

import { VCDateTime } from "./VCDateTime";
import {
    CustomAddressInput,
    CustomDateInput,
    CustomEmailInput,
    CustomGenderInput,
    CustomInput,
    CustomPhoneInput,
    CustomTextAreaInput,
    CustomTextInput,
    CustomUriInput,
    CustomNameInput,
    CustomImageInput,
    CustomGeoInput,
    CustomSocialInput,
    CustomTimezoneInput,
    CustomLangInput,
    CustomFiyUserInput,
} from "./CustomInput";

type VCValueType = 'boolean' | 'DATE' | 'DATE-AND-OR-TIME' | 'DATE-TIME'
    | 'FLOAT' | 'INTEGER' | 'LANGUAGE-TAG' | 'TEXT' | 'TIME' | 'TIMESTAMP'
    | 'URI' | 'UTC-OFFSET' | string;

/// Params for a property

interface VCParams {
    [k: string]: any;

    // +-----------+-----------+------------------------+
    // | Namespace | Parameter | Reference              |
    // +-----------+-----------+------------------------+
    // |           | LANGUAGE  | RFC 6350, Section 5.1  |
    // |           | VALUE     | RFC 6350, Section 5.2  |
    // |           | PREF      | RFC 6350, Section 5.3  |
    // |           | ALTID     | RFC 6350, Section 5.4  |
    // |           | PID       | RFC 6350, Section 5.5  |
    // |           | TYPE      | RFC 6350, Section 5.6  |
    // |           | MEDIATYPE | RFC 6350, Section 5.7  |
    // |           | CALSCALE  | RFC 6350, Section 5.8  |
    // |           | SORT-AS   | RFC 6350, Section 5.9  |
    // |           | GEO       | RFC 6350, Section 5.10 |
    // |           | TZ        | RFC 6350, Section 5.11 |
    // +-----------+-----------+------------------------+


    /// Language for value
    LANGUAGE?: string;

    /// Value type for the field
    // +------------------+-------------------------+
    // | Value Data Type  | Reference               |
    // +------------------+-------------------------+
    // | BOOLEAN          | RFC 6350, Section 4.4   |
    // | DATE             | RFC 6350, Section 4.3.1 |
    // | DATE-AND-OR-TIME | RFC 6350, Section 4.3.4 |
    // | DATE-TIME        | RFC 6350, Section 4.3.3 |
    // | FLOAT            | RFC 6350, Section 4.6   |
    // | INTEGER          | RFC 6350, Section 4.5   |
    // | LANGUAGE-TAG     | RFC 6350, Section 4.8   |
    // | TEXT             | RFC 6350, Section 4.1   |
    // | TIME             | RFC 6350, Section 4.3.2 |
    // | TIMESTAMP        | RFC 6350, Section 4.3.5 |
    // | URI              | RFC 6350, Section 4.2   |
    // | UTC-OFFSET       | RFC 6350, Section 4.7   |
    // +------------------+-------------------------+
    VALUE?: VCValueType;

    /// Preference level 0-100
    PREF?: number;

    /// Used for language overloading
    ALTID?: number;

    /// Used to identify specific properties among duplicates
    PID?: string[];

    /// Various uses (ie - home, work, fax, etc.)
    TYPE?: string;

    /// Content-Type
    MEDIATYPE?: string;

    //
    CALSCALE?: string;

    // b/BASE64
    ENCODING?: string;
}

interface VCPropSpec {
    name: string,
    description: string,
    type: string,
    versions: boolean | string[],
    const?: boolean,
    internal?: boolean,
    customInput?: any,
    example?: string,
}

/// Valid vcard properties
const vCardProperties: { [k: string]: VCPropSpec } = {
    'ADR': {
        name: 'Address',
        description: 'Physical delivery address',
        type: 'address',
        versions: true,
        customInput: CustomAddressInput,
    },
    'AGENT': {
        name: 'Agent',
        description: 'Person who will act on their behalf',
        type: 'uri',
        versions: ['2.1', '3.0'],
    },
    'ANNIVERSARY': {
        name: 'Anniversary',
        description: 'Anniversary Date',
        type: 'date-and-or-time',
        versions: ['4.0'],
        customInput: CustomDateInput,
    },
    'BDAY': {
        name: 'Birthday',
        description: 'Birthday Date',
        type: 'date-and-or-time',
        versions: true,
        customInput: CustomDateInput,
    },
    'CALURI': {
        name: 'Calendar Link',
        description: 'Link to calender for sending scheduling requests',
        type: 'uri',
        versions: ['4.0'],
    },
    'CATEGORIES': {
        name: 'Categories',
        description: 'List of descriptive tags',
        type: 'text', // comma separated tags
        versions: ['3.0', '4.0'],
    },
    'CLASS': {
        name: 'Class',
        description: 'Describes sensitivity of this information',
        type: 'text',
        versions: ['3.0'],
    },
    'CLIENTPIDMAP': {
        name: 'Revision History',
        description: 'Not supported',
        internal: true,
        type: 'text',
        versions: false,
    },
    'EMAIL': {
        name: 'Email',
        description: 'Email address',
        type: 'text', // does not include mailto:
        versions: true,
        customInput: CustomEmailInput,
    },
    'FBURL': {
        name: 'Free-Busy URL',
        description: 'URL that shows when person is "free" or "busy"',
        type: 'uri',
        versions: ['4.0'],   
    },
    'FN': {
        name: 'Name',
        description: 'Full name, as it should be formatted',
        type: 'text',
        versions: true,
        customInput: CustomTextInput,
    },
    'GENDER': {
        name: 'Gender',
        description: 'Describes the person\s gender',
        type: 'sex',
        versions: ['4.0'],
        customInput: CustomGenderInput,
    },
    'GEO': {
        name: 'Geographic Location',
        description: 'Related latitude and longitude',
        type: 'geo', // 4.0: geo:lat,lon || 2.1,3.0: geo:lat;lon
        versions: ['2.1','3.0','4.0'],
        customInput: CustomGeoInput,
    },
    'IMPP': {
        name: 'Instant Messenger Handle',
        description: 'Instant messenger handle',
        type: 'uri',
        versions: ['3.0', '4.0'],
        customInput: CustomUriInput,
    },
    'KEY': {
        name: 'Public key',
        description: 'Associated cryptographic key for encryption',
        type: 'key',
        versions: ['2.1','3.0','4.0'],
    },
    'KIND': {
        name: 'Entity Kind',
        description: 'Type of entity that this card refers to',
        type: 'text', // allowed: 'application', 'individual', 'group', 'location' or 'organization'; 'x-*'
        versions: ['4.0'],
    },
    'LABEL': {
        name: 'Shipping Address',
        description: 'Text that should go on a mailing label',
        type: 'address', // same as ADR property
        versions: ['2.1', '3.0'],
        // version 4.0: ADR;LABEL: ...
    },
    'LANG': {
        name: 'Language',
        description: 'Language that a person speaks',
        type: 'lang', // Language code: en-US, fr-CA
        versions: true,
        customInput: CustomLangInput,
    },
    'LOGO': {
        name: 'Logo',
        description: 'Logo for associated organization',
        type: 'img', // either encoded image or url
        versions: ['2.1','3.0','4.0'],
        customInput: CustomImageInput,
    },
    'MAILER': {
        name: 'Email App',
        description: 'Email App Used',
        type: 'text',
        versions: ['2.1','3.0'],
    },
    'MEMBER': {
        name: 'Member of this Group',
        description: 'Defines a member of the group that this card represents',
        type: 'uri',
        versions: ['4.0'],
    },
    'N': {
        name: 'Structured Name',
        description: 'Structured representation of the person\'s name',
        type: 'name', // name
        versions: true,
        customInput: CustomNameInput,
    },
    'NICKNAME': {
        name: 'Nickname',
        description: 'More descriptive/familiar names',
        type: 'text', // comma separated list
        versions: true,
        customInput: CustomTextInput,
    },
    'NOTE': {
        name: 'Notes',
        description: 'Supplimental information',
        type: 'text',
        versions: true,
        customInput: CustomTextAreaInput,
    },
    'ORG': {
        name: 'Organization',
        description: 'Units of associated organization',
        type: 'text',
        versions: true,
        customInput: CustomTextInput,
    },
    'PHOTO': {
        name: 'Photo',
        description: 'Photo of individual/profile picture',
        type: 'img',
        versions: true,
        customInput: CustomImageInput,
    },
    'PRODID': {
        name: 'Created by',
        description: 'Product that created this card',
        type: 'text',
        internal: true,
        const: true,
        versions: ['3.0','4.0'],
    },
    'PROFILE': {
        name: 'File type',
        description: 'vcard',
        type: 'text', // vcard
        versions: ['3.0'],
    },
    'RELATED': {
        name: 'Related',
        description: 'Related entity',
        type: 'text',
        versions: ['4.0'],
    },
    'REV': {
        // logic for this should be on frontend...
        name: 'Updated',
        description: 'Last time this contact was edited',
        type: 'timestamp',
        versions: true,
        const: true,
    },
    'ROLE': {
        name: 'Role',
        description: 'Role, Occupation or Business category',
        type: 'text',
        versions: true,
        customInput: CustomTextInput,
    },
    'SORT-STRING': {
        name: 'Sort By',
        description: 'Sort',
        type: 'text',
        versions: ['3.0'],
    },
    'SOURCE': {
        name: 'vCard Source',
        description: 'Where to download this vCard contact file',
        type: 'uri',
        versions: ['3.0','4.0'],
    },
    'SOUND': {
        name: 'Name Pronounciation',
        description: 'How to pronounce their name',
        type: 'text',
        versions: true,
    },
    'TEL': {
        name: 'Phone Number',
        description: 'Phone number',
        type: 'text', // note: can be uri
        versions: true,
        customInput: CustomPhoneInput,
    },
    'TITLE': {
        name: 'Job Title',
        description: 'Job title, position, or function',
        type: 'text',
        versions: true,
        customInput: CustomTextInput,
    },
    'TZ': {
        name: 'Timezone',
        description: 'Associated timezone',
        type: 'timezone',
        versions: true,
        customInput: CustomTimezoneInput,
    },
    'UID': {
        name: 'Contact ID',
        description: 'instance unique contact id',
        type: 'text',
        versions: true,
        const: true,
    },
    'URL': {
        name: 'Website',
        description: 'URL to website that represents the contact',
        type: 'uri',
        versions: true,
        customInput: CustomUriInput,
    },
    'VERSION': {
        name: 'vCard Version',
        description: 'vCard File Version',
        type: 'text',
        versions: true,
    },

    // Non-standard properties
    'X-SOCIALPROFILE': {
        name: 'Social Media Profile',
        description: 'Link to social media profile page',
        type: 'uri',
        versions: true,
        customInput: CustomSocialInput,
    },
    'X-FIY-PROFILE': {
        name: 'FIY User Profile',
        description: 'FIY user who owns this profile',
        type: 'text',
        versions: true,
        customInput: CustomFiyUserInput,
        // If user changes the fiy-profile on their profile
        // the backend will simply make a new one as needed
        // so it's safe to make it editable
    }
};

/// Part of a vCard
export class VCProperty {

    static known = vCardProperties;

    /**
     * @param name vcard property name
     * @param params vcard parameters
     * @param value raw vcard value
     */
    constructor(
        public name: string,
        public params: VCParams = {},
        public value: string = "",
    ) {
    }

    ////
    // vCard methods
    ////

    /**
     * Gets the property name, excluding group name
     */
    getName() {
        return this.name.split('.').pop()
    }

    paramsString() {
        return Object.entries(this.params).map(([p, v]) => {
            switch (typeof v) {
                case 'string': case 'number':
                    return `${p}=${v}`;
                case 'object':
                    return `${p}=${v.join(',')}`;
                case 'undefined':
                    return '';
                default:
                    return p;
            }
        }).filter(p=> !!p).join(';');
    }

    toLine(): string {
        const params = this.paramsString();
        return `${this.name}${
            params ? ';' + params : ''            
        }:${
            this.value
        }`;
    }

    static fromLine(line: string): VCProperty | null {
        const colon = line.indexOf(':');
        const value = line.slice(colon+1);
        const [key, ...paramStrs] = line
            .slice(0, colon)
            .split(';')
            .map(s => s.trim());

        if (["BEGIN", "END"].includes(key))
            return null;

        if (key === 'VCARD')
            return null;

        const params: VCParams = {};

        paramStrs.forEach(p => {
            const [k,v] = p.split('=');
            if (v)
                params[k] = v;
            // else if (!vcDefaultParams.includes(k))
            //     params['TYPE'] = k;
        });

        return new VCProperty(key, params, value);
    }

    ////
    // Value methods
    ////

    fromJSDate(d: Date) {
        this.fromVCDate(
            d.getUTCDate().toString(),
            d.getUTCMonth().toString(),
            d.getUTCFullYear().toString()
        );
    }

    fromVCDate(day?: string, month?: string, year?: string) {
        if (year && year.length < 4)
            year = '000'.slice(year.length - 4);
        if (year && year.length > 4) {
            console.error('year > 10000');
            year = '+' + year; // ISO-8601 spec but this will probably not work anywhere lol
        }
        let ret = year || '--';
        if (month && month.length == 1)
            ret += '0';
        ret += month || '-';
        if (!month)
            day = undefined;
        if (day && day.length == 1)
            ret += '0';
        ret += day || '';
        this.value = ret;
    }

    /**
     * Converts a N property value to a human readable string
     */
    nameToString() {
        const [
            surnames,
            givenNames,
            additionalNames,
            prefixes,
            suffixes
        ] = this.value
            .split(';')
            .map(n => n.split(','));
        return [(prefixes?.join(' ') || ''),
            (givenNames?.join(' ') || ''),
            (additionalNames?.join(' ') || ''),
            (surnames?.join(' ') || ''),
            (suffixes?.join(' ') || '')
        ].filter(n => !!n).join(' ');
    }

    /**
     * Gender, this documentation is what the spec says, I'm going to prompt something different though.
     * @param sex A single letter representing biological sex.  M stands for "male", F stands
     *    for "female", O stands for "other", N stands for "none or not
     *    applicable", U stands for "unknown".
     * @param identity Gender identity component: free-form text
     */
    fromGender(sex: "" | "M" | "F" | "O" | "N" | "U" = "", identity?: string) {
        this.value = sex;
        if (identity) 
            this.value += ';' + identity;
    }

    // Family Name, Given Name, Additional/Middle Names, Honorific Prefixes, and Honorific Suffixes
    fromName(
        surname: string,
        givenName: string,
        middleNames: string[] = [],
        prefixes: string[] = [],
        suffixes: string[] = [],
    ) {
        this.value = `${surname};${
            givenName};${
                middleNames.join(',')
            };${
                prefixes.join(',')
            };${
                suffixes.join(',')
            }`;
    }

    fromCoords(lat: number, lng: number, version) {
        if (version === '2.1' || version === '3.0')
            this.value = `${lat},${lng}`;
        else
            this.value = `geo:${lat},${lng}`;
    }

    ////
    // UI methods
    ////

    input?: CustomInput;

    label() {
        return vCardProperties[this.getName()]?.name || this.name;
    }

    valueHtml() {
        switch (vCardProperties[this.getName()]?.type) {
            case 'uri':
                return `<a href="${this.value}" target="_blank">${this.value}</a>`;
            case 'name':
                return this.nameToString();

            case 'date-and-or-time':
            case 'timestamp':
                return VCDateTime.parse(this.value).toUserString();

            case 'img':
                if (this.params.ENCODING === 'b')
                    return `<img src="data:image/${this.params.TYPE};base64, ${this.value}" alt="PHOTO" />`;
                else
                    return `<img src="${this.value}" alt="PHOTO" />`;

            case 'key':
                /*
                2.1: KEY;PGP:http://example.com/key.pgp
                2.1: KEY;PGP;ENCODING=BASE64:[base64-data]
                3.0: KEY;TYPE=PGP:http://example.com/key.pgp
                3.0: KEY;TYPE=PGP;ENCODING=b:[base64-data]
                4.0: KEY;MEDIATYPE=application/pgp-keys:http://example.com/key.pgp
                4.0: KEY:data:application/pgp-keys;base64,[base64-data]
                */
                if (this.value.startsWith('http'))
                    return `<a href="${this.value}" target="_blank">Download key</a>`;
                if (this.value.startsWith('data:'))
                    return `<a href="${this.value}" target="_blank" download="key${
                        this.value.includes('pgp') ? '.pgp' : ''}">Download key</a>`;

                // Here there be dragons
                // not sure what content type the file should have
                // we'll let their system figure it out
                // Assuming the data is b64 encoded
                return `<a href="data:application/octet-stream;base64,${
                    this.value}" target="_blank" download="key">Download key</a>`;

            // TODO img, address

            case 'sex':
            case 'text':
            case 'timezone':
            case 'lang':
            default:
                return this.value;
        }
    }

    showHtml() {
        let paramsHtml = "";
        if (this.params.TYPE)
            paramsHtml += `<h5>Type: ${this.params.TYPE}</h5>`;

        return `<h4 title="${this.name}">${this.label()}</h4>${paramsHtml}${this.valueHtml()}</div>`
    }

    inputHtml(e: HTMLElement) {
        if (!this.input) {
            const InputConstructor = vCardProperties[this.getName()]?.customInput;
            if (InputConstructor)
                this.input = new InputConstructor(this);
        }
        if (this.input) {
            e.insertAdjacentHTML('beforeend', `<h4>${this.label()}</h4>`);
            this.input.html(e);
        } else {
            e.insertAdjacentHTML('beforeend', this.showHtml());
        }
    }

    async acceptInput() {
        if (this.input)
            await this.input.loadValue();
    }
}
