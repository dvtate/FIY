//
// vCard
//
import { VCProperty } from "./VCProperty";
import * as API from './api';

/// vCard entity
export default class VC {
    public static readonly BEGIN_TOKEN = "BEGIN:VCARD";
    public static readonly END_TOKEN = "END:VCARD";

    // Set to true when
    isUnsaved: boolean = false;

    properties: VCProperty[] = [];

    addProperty(property: VCProperty) {
        this.properties.push(property);
    }

    private displayNameCache: string;

    displayName() {
        if (this.displayNameCache)
            return this.displayNameCache;

        let p = this.properties.find(p => p.name === 'FN');
        if (p)
            return this.displayNameCache = p.value;

        p = this.properties.find(p => p.name === 'N');
        if (p)
            return this.displayNameCache = p.nameToString();

        // User is creating a new contact
        if (this.isUnsaved)
            return "New Unsaved Contact";

        // Either N or FN is required by the vCard spec
        // So this card is invalid
        return "Invalid: No Name";
    }

    getId(): string {
        // Try card UID field
        let p = this.properties.find(p => p.name == "UID");
        if (p)
            return p.value;

        // Looks like "SOURCE:<protocol><domain>/contacts/id/<contact id>"
        p = this.properties.find(p => p.name == 'SOURCE');
        if (!p)
            return '-1';
        return p.value.split('/').pop();
    }

    getFiyUser() {
        let p = this.properties.find(p => p.name == 'X-FIY-PROFILE');
        if (!p)
            return '';
        let ret = p.value.trim();
        if (ret == '')
            return '';
        if (!p.value.includes('@'))
            ret += '@' + API.domain;
        return ret;
    }

    getProperty(property: string) {
        return this.properties.find(p => p.name == property);
    }

    static parseCard(vc: string) {
        // Not a vcard
        vc = vc.trim();
        if (!vc.startsWith('BEGIN:VCARD'))
            return null;

        const ret = new VC();
        // const props = vc.split(/\r\n|\n\S/);
        const props = vc
            // replace escaped new lines
            .replace(/\n\s{1}/g, '')
            // split if a character is directly after a newline
            .split(/\r\n(?=\S)|\r(?=\S)|\n(?=\S)/);
        props.forEach(p => {
            const vcp = VCProperty.fromLine(p);
            if (vcp != null)
                ret.properties.push(vcp);
        });

        return ret;
    }

    static parseCards(vcf: string): VC[] {
        return vcf.split('END:VCARD')
            .slice(0, -1)
            .map(s => VC.parseCard(s.trim()))
            .filter(vc => !!vc);
    }

    /**
     * Convert to vCard v4.0 representation
     * Remove all properties that get set by the server
     */
    convertInternal() {
        // TODO this logic should also be on the backend

        // Remove some properties
        this.properties = this.properties.filter(p =>
            ![
                // Obsolete/pointless
                'AGENT',
                'CLASS',
                'MAILER',
                'PROFILE',
                'SORT-STRING',

                // Not supported
                'CLIENTPIDMAP',

                // Internal only
                'UID',
            ].includes(p.name)
        );

        // Convert images to modern format
        this.properties = this.properties.filter(p => {
            if (!['PHOTO', 'LOGO'].includes(p.name))
                return true;

            const encoding = p.params.ENCODING || p.params.encoding;
            if (!encoding)
                return true;

            if (encoding !== 'b') {
                console.warn(`${p.name}: invalid ENCODING: ${encoding}`);
                return false;
            }
            const validImgTypes = ['png', 'gif', 'jpeg', 'webp', 'bmp', 'x-icon'];
            const type = Object.keys(p.params).find(p =>
                    validImgTypes.includes(p.toLowerCase())
                ) || p.params.TYPE || p.params.type || '';
            if (!type) {
                console.warn(`${p.name}: couldn't determine type: ${p.toLine()}`);
            }

            delete p.params.encoding;
            delete p.params.ENCODING;
            if (type)
                delete p.params[type];

            if (p.value.startsWith('data:image/'))
                return true;
            if (p.value.startsWith('http')) {
                if (type)
                    // p.params.MEDIATYPE = type.startsWith('image/')
                    //     ? type
                    //     : `image/${type}`;
                return true;
            }
            p.value = `data:image/${type};base64,${p.value}`;
            return true;
        });

        // Convert LABEL into ADR;LABEL:
        this.properties
            .filter(p => p.name === 'LABEL')
            .forEach(p => {
                p.name = 'ADR';
                if (p.params.TYPE)
                    p.params.TYPE += ',LABEL';
                else
                    p.params.TYPE = 'LABEL';
            });

        const version = this.properties.find(p => p.name === 'VERSION');
        let oldVersion = '4.0';
        if (!version) {
            this.properties.unshift(new VCProperty('VERSION', {}, '4.0'));
        } else {
            oldVersion = version.value;
            version.value = '4.0';
        }

        // If it's really old, only include known properties
        // if (!['4.0', '3.0'].includes(oldVersion))
            this.properties = this.properties.filter(p => VCProperty.known[p.name]);

        return this;
    }

    /**
     * Get .vcf file contents as a string
     */
    vCardString() {
        const version = this.properties.find(p =>
                p.name === 'VERSION')?.toLine() || "VERSION:4.0";

        let ret = 'BEGIN:VCARD\r\n';
        ret += version + '\r\n';
        ret += this.properties
            .map(p => p.name === 'VERSION' ? '' : p.toLine())
            .join('\r\n');
        ret += '\r\nEND:VCARD';
        return ret;
    }

    /**
     * Returns the user's username if this card is a profile card, otherwise gives false
     */
    isProfileCard() {
        // For the user's profile card, the server sets a special property:
        //      X-FIY-PROFILE: username
        const p = this.properties.find(p => p.name === 'X-FIY-PROFILE');
        if (!p)
            return false;
        return p.value;
    }

    /**
     * Gives a representation of the vCard property groups for this card
     *
     * @notes I don't want to support contact groups
     *      but apple,google,etc. abuse them
     */
    propertiesGrouped(): { [group: string] : VCProperty | VCProperty[] } {
        /*
        {
            0: property,
            'group': [ property, property ],
            3: property,
            ...
        }
         */
        let ret = {};
        for (let i = 0; i < this.properties.length; i++) {
            if (this.properties[i].name.includes('.')) {
                const group = this.properties[i].name.split('.')[0];
                if (!ret[group])
                    ret[group] = [];
                ret[group].push(this.properties[i]);
            } else {
                ret[i] = this.properties[i];
            }
        }
        return ret;
    }

    static readonly defaultPfp = "data:image/png;base64,"
        + "iVBORw0KGgoAAAANSUhEUgAAAQAAAAEAAQMAAABmvDolAAAAA1BMVEW10NBjB"
        + "BbqAAAAH0lEQVRoge3BAQ0AAADCoPdPbQ43oAAAAAAAAAAAvg0hAAABmmDh1Q"
        + "AAAABJRU5ErkJggg==";

    profilePhotoUri() {
        for (const p of this.properties)
            if (p.name === 'PHOTO' && p.value)
                return p.params.ENCODING === 'b'
                    ? `data:${p.params.TYPE};base64,${p.value}`
                    : p.value;
        // default pfp: square
        return VC.defaultPfp;
    }

    showHtml() {
        const dn = this.isProfileCard()
            ? 'Your Profile'
            : this.displayName();
        const fiyUser = this.getFiyUser();
        const propsHtml = Object.entries(this.propertiesGrouped())
            .map(([k, v]) =>
                v instanceof VCProperty
                    ? v.showHtml()
                    : `<fieldset><legend>${k}</legend>${
                        v.map(v => v.showHtml()).join('<hr/>')
                        }</fieldset>`
            ).join('<hr/>');

        return `<h2>${dn}</h2>${
            fiyUser ? `<h3>${fiyUser}</h3>` : ''
        }${
            propsHtml
        }`;
    }

    addEditHtml(e: HTMLElement) {
        e.innerHTML = `<h2>Editing Contact</h2><hr>`;

        const form = document.createElement('div');
        e.appendChild(form);

        const addDeleteButton = (p: VCProperty, i: number) => {
            form.insertAdjacentHTML(
                'beforeend',
                `<br><button id="edit-p-del-${i}"><i class="fa fa-trash"></i> Remove</button>`,
            );
            document.getElementById('edit-p-del-' + i).addEventListener('click', event => {
                this.properties = this.properties.filter(prop => prop !== p);
                this.addEditHtml(e);
            });
        }
        const addVisibilitySelect = this.isProfileCard()
            ? (p: VCProperty, i: number) => {
                form.insertAdjacentHTML('beforeend', `<select id="edit-vis-${i}">
                    <option value="0">Only Me</option>
                    <option value="1">Local ${API.domain} users</option>
                    <option value="2">Any federated user</option>
                    <option value="3">Public</option>
                </select>`);
                const visSelect = document.getElementById('edit-vis-' + i) as HTMLSelectElement;
                visSelect.value = p.params['X-FIY-VISIBILITY'] || '3';
            }
            : (p: VCProperty, i: number) => {};

        this.properties.forEach((p, i) => {
            p.inputHtml(form);

            if (p.name !== 'FN')
                addDeleteButton(p, i);
            addVisibilitySelect(p, i);

            form.insertAdjacentHTML('beforeend', '<hr>');
        });

        // Button to add a property to the contact
        const optionsHtml = Object.entries(VCProperty.known)
            .filter(([k, v]) =>
                v.customInput && (typeof v.versions == 'boolean'
                    ? v.versions
                    : v.versions.includes('4.0')))
            .map(([k, v]) =>
                `<option value="${k}" title="${v.description}">${v.name}</option>`)
            .join('');
        e.insertAdjacentHTML('beforeend',
            `<label for="add-prop-type"><h4>Add Field</h4></label>
            <div class="input-group"><select id="add-prop-type">${optionsHtml}</select>
            <button id="add-prop-btn"><i class="fa fa-plus"></i>Add</button>
        </div>`);

        // Add property editor to the form
        document.getElementById('add-prop-btn').addEventListener('click', event => {
            const typeSelect = document.getElementById('add-prop-type') as HTMLSelectElement;
            const p = new VCProperty(typeSelect.value)
            this.properties.push(p);
            p.inputHtml(form);
            addDeleteButton(p, this.properties.length - 1);
            addVisibilitySelect(p, this.properties.length - 1);
            form.insertAdjacentHTML('beforeend', '<hr>');
        });
    }

    download() {
        const e = document.createElement('a');
        e.setAttribute('href', 'data:text/vcard;base64,' + btoa(this.vCardString()));
        e.setAttribute('download', 'contact.vcf');
        e.style.display = 'none';
        document.body.appendChild(e);
        e.click();
        document.body.removeChild(e);
    }

    validationErrors() {
        return this.properties.map( p => [p.name, p.input?.validate()]).filter(e => !!e[1]).map(e => e[0] + ':' + e[1]);
    }

    async acceptEdits() {
        const isProfileCard = !!this.isProfileCard();
        await Promise.all(this.properties.map(async (p, i) => {
            await p.acceptInput();
            if (isProfileCard) {
                const visSelect = document.getElementById('edit-vis-' + i) as HTMLSelectElement;
                if (visSelect)
                    p.params['X-FIY-VISIBILITY'] = visSelect.value;
                else
                    console.log('no visibility selector', p.name, i);
            }
        }));
    }

    sortCompare(other: VC) {
        // TODO vCard has rules for sorting we probably should follow
        return this.displayName().localeCompare(other.displayName());
    }
}