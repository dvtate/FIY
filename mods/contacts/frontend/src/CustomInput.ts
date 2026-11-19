//
// Procedurally generated inputs for editing vCard fields
//

import { VCProperty } from "./VCProperty";
import { VCDateTime } from "./VCDateTime";
import { getTzDb, domain } from "./api";

/*

+------------------+-------------------------+
| Value Data Type  | Reference               |
+------------------+-------------------------+
| BOOLEAN          | RFC 6350, Section 4.4   |
| DATE             | RFC 6350, Section 4.3.1 |
| DATE-AND-OR-TIME | RFC 6350, Section 4.3.4 |
| DATE-TIME        | RFC 6350, Section 4.3.3 |
| FLOAT            | RFC 6350, Section 4.6   |
| INTEGER          | RFC 6350, Section 4.5   |
| LANGUAGE-TAG     | RFC 6350, Section 4.8   |
| TEXT             | RFC 6350, Section 4.1   |
| TIME             | RFC 6350, Section 4.3.2 |
| TIMESTAMP        | RFC 6350, Section 4.3.5 |
| URI              | RFC 6350, Section 4.2   |
| UTC-OFFSET       | RFC 6350, Section 4.7   |
+------------------+-------------------------+
*/

export abstract class CustomInput {

    protected static _uuid = 0;
    static uid(): string {
        return 'pg-input' + ++CustomInput._uuid;
    }

    constructor(
        public property: VCProperty,
        public id: string = CustomInput.uid(),
    ) {}

    /**
     * Validate user input
     * @return erorr message if invalid
     */
    abstract validate(): string | null;

    /**
     * Add input html to element
     * @param e
     */
    abstract html(e: HTMLElement): void;

    /**
     * Update value of relevant VCProperty
     */
    abstract loadValue(): void;
}

export class CustomDateInput extends CustomInput {
    validate(): string | null {
        // const e = document.getElementById(this.id) as HTMLInputElement;
        // const v = e.value;
        return null;
    }

    initialValue() {
        // Use existing value if available
        if (this.property.value) {
            const o = VCDateTime.parse(this.property.value);
            const today = new Date().toISOString().slice(0,10);
            if (!o.year)
                o.year = today.slice(0,4);
            if (!o.month)
                o.month = today.slice(5, 7);
            if (!o.day)
                o.day = today.slice(8, 10);
            return `${o.year}-${o.month}-${o.day}`;
        }

        // Default to current date
        return VCDateTime.now().toVCString().split('T')[0];
    }

    html(e: HTMLElement) {
        e.insertAdjacentHTML('beforeend', `<div class="input-group">
    <label for="${this.id}">${this.property.label()}</label>
    <input type="date" id="${this.id}" value="${this.initialValue()}">
</div>`);
        document.getElementById(this.id).addEventListener('change', this.validate.bind(this));
    }

    loadValue() {
        const e = document.getElementById(this.id) as HTMLInputElement;

        // format YYYY-MM-DD -> YYYYMMDD
        this.property.value = e.value.replaceAll('-','');
    }
}

export class CustomEmailInput extends CustomInput {
    validate(): string | null {
        const e = document.getElementById(this.id) as HTMLInputElement;
        if (!/[^\s@]@[^\s@]/.test(e.value.trim())) {
            e.style.border = '1px solid red';
            return 'Invalid email';
        }
        e.style.border = 'default';
        return null;
    }

    typeOptionsHtml() {
        const selected = this.property.value;
        let ret = '';
        if (selected)
            ret += `<option value="${selected}" selected>${selected}</option>`;

        return ret + `<option value="PERSONAL">Personal</option>
<option value="WORK">Work</option>
<option value="HOME">Home</option>
<option value="OTHER">Other</option>`;
    }

    html(e: HTMLElement) {
        e.insertAdjacentHTML(
            'beforeend',
            `<div class="input-group">
<label for="${this.id}">Email</label>
<input id="${this.id}" type="email" value="${this.property.value || ''}" />
</div><div class="input-group">
<label for="${this.id}-type">Type</label>
<select id="${this.id}-type">${this.typeOptionsHtml()}</select>
</div>`);
        document.getElementById(this.id)
            .addEventListener('change', this.validate.bind(this));
    }

    loadValue() {
        const v = document.getElementById(this.id) as HTMLInputElement;
        const t = document.getElementById(this.id + '-type') as HTMLInputElement;
        this.property.value = v.value;
        this.property.params.TYPE = t.value;
    }
}

export class CustomTextInput extends CustomInput {
    getValue() {
        const e = document.getElementById(this.id) as HTMLInputElement;
        return e.value;
    }
    override validate(): string | null {
        return this.getValue().length !== 0 ? null : "Value cannot be empty";
    }

    placeholderText(): string {
        return '';
    }

    defaultValue(): string {
        return this.property.value || '';
    }

    override html(e: HTMLElement): void {
        e.insertAdjacentHTML('beforeend',
            `<div class="input-group"><label for="${this.id}">${
            this.property.label()}</label><input type="text" id="${this.id}" value="${
            this.defaultValue()}" placeholder="${this.placeholderText()
            }" /></div>`
        );
    }

    override loadValue(): void {
        this.property.value = this.getValue();
    }
}

export class CustomTextAreaInput extends CustomTextInput {
    override html(e: HTMLElement): void {
        e.insertAdjacentHTML('beforeend', `<div class="input-group">
    <label for="${this.id}">${this.property.label()}</label>
    <textarea id="${this.id}" rows="3">${this.property.value ||''}</textarea>
</div>`);
    }
}

export class CustomUriInput extends CustomTextInput {

    override placeholderText(): string {
        return 'https://example.com';
    }

    override html(e: HTMLElement): void {
        e.insertAdjacentHTML('beforeend', `<div class="input-group">
    <label for="${this.id}">${this.property.label()}</label>
    <input type="url" id="${this.id
        }" value="${this.property.value || ''
        }" placeholder="${this.placeholderText()}" />
</div>`);
    }
}

export class CustomSocialInput extends CustomUriInput {
    override placeholderText(): string {
        return 'https://x.com/x';
    }
}

// Should this instead accept a URI input?
export class CustomPhoneInput extends CustomInput {
    getValue() {
        const e = document.getElementById(this.id) as HTMLInputElement;
        return e.value;
    }

    override validate(): string | null {
        return this.getValue().length !== 0 ? null : "Phone number cannot be empty";
    }

    typeOptionsHtml() {
        // TODO replace this with checkboxes or something and combine with commas
        const selected = this.property.params.TYPE;
        let ret = '';
        if (selected)
            ret += `<option value="${selected}" selected>${selected}</option>`;

        // TODO probably more options
        return ret + `<option value="voice" title="Supports voice calling">Voice</option>
<option value="text" title="SMS communications supported">Texting</option>
<option value="cell" title="Mobile telephone">Cell</option>
<option value="video" title="Video calling supported">Video</option>
<option value="textphone" title="Accessibility enabled phone">Textphone</option>
<option value="pager" title="Pager device, limited functionality">Pager</option>
<option value="fax" title="Fax machine">Fax</option>`;
    }

    override html(e: HTMLElement) {
        e.insertAdjacentHTML('beforeend', `<div class="input-group">
    <label for="${this.id}">Phone number</label>
    <input id="${this.id}" type="tel" placeholder="+15558675309" value="${this.property.value || ''}" />
</div><div class="input-group">
    <label for="${this.id}-type">Type</label>
    <select id="${this.id}-type">${this.typeOptionsHtml()}</select>
</div>`);
        document.getElementById(this.id)
            .addEventListener('change', this.validate.bind(this));
    }

    override loadValue() {
        const v = document.getElementById(this.id) as HTMLInputElement;
        const t = document.getElementById(this.id + '-type') as HTMLInputElement;
        this.property.value = v.value;
        this.property.params.TYPE = t.value;
    }
}

export class CustomGenderInput extends CustomInput {
    override validate(): string | null {
        return null; // All genders are valid :)
    }

    override html(e: HTMLElement): void {
        const pv = this.property.value.split(';');
        const sex = (pv[0] || '').toUpperCase();
        const gender = pv[1] || '';

        // Sex selector
        const sexSelect = `<div class="input-group">
        <label for="${this.id}-sex" title="RFC 6350: Sex (biological)">Sex</label>
        <select id="${this.id}-sex">
            <option value="" ${sex === '' ? 'selected' : ''}>Select a sex</option>
            <option value="M" ${sex === 'M' ? 'selected' : ''}>Male</option>
            <option value="F" ${sex === 'F' ? 'selected' : ''}>Female</option>
            <option value="O" ${sex === 'O' ? 'selected' : ''}>Other</option>
            <option value="U" ${sex === 'U' ? 'selected' : ''}>Unknown</option>
            <option value="N" ${sex === 'N' ? 'selected' : ''}>None</option>
        </select></div>`;

        // Gender identity
        const genderInfo = `<div class="input-group">
            <label for="${this.id}-gender" title="RFC 6350: Gender identity">Gender identity/info</label>
            <input type='text' id="${this.id}-gender" value="${gender}" />
        </div>`;

        e.insertAdjacentHTML('beforeend', `${sexSelect}${genderInfo}`);
    }

    override loadValue(): void {
        const s = document.getElementById(this.id + '-sex') as HTMLInputElement;
        const g = document.getElementById(this.id + '-gender') as HTMLInputElement;
        this.property.value = s.value;
        if (g.value)
            this.property.value += ';' + g.value;
    }
}

export class CustomNameInput extends CustomInput {
    override validate() {
        return null;
    }

    override html(e: HTMLElement): void {
        /*
            The structured property value corresponds, in
            sequence, to the Family Names (also known as surnames), Given
            Names, Additional Names, Honorific Prefixes, and Honorific
            Suffixes.  The text components are separated by the SEMICOLON
            character (U+003B).  Individual text components can include
            multiple text values separated by the COMMA character (U+002C).
            This property is based on the semantics of the X.520 individual
            name attributes [CCITT.X520.1988].  The property SHOULD be present
            in the vCard object when the name of the object the vCard
            represents follows the X.520 model.
        */

        // Get values from property
        const v = this.property.value || ';;;;';
        const names = v.split(';').map(c => c.split(','));
        const surnames = names[0] && names[0].join(' ') || '';
        const givenNames = names[1] && names[1].join(' ') || '';
        const additionalNames = names[2] && names[2].join(' ') || '';
        const prefixes = names[3] && names[3].join(' ') || '';
        const suffixes = names[4] && names[4].join(' ') || '';

        // Add form
        let html = `<div class="input-group" title="Family name(s) aka surname(s), separated with spaces">
            <label for="${this.id}-surnames">Surname(s)</label>
            <input type="text" id="${this.id}-surnames" value="${surnames}" />
        </div><div class="input-group" title="Given names">
            <label for="${this.id}-given">Given Name(s)</label>
            <input type="text" id="${this.id}-given" value="${givenNames}" />
        </div><div class="input-group" title="Additional names">
            <label for="${this.id}-additional">Additional Name(s)</label>
            <input type="text" id="${this.id}-additional" value="${additionalNames}" />
        </div><div class="input-group" title="Honorific Prefixes">
            <label for="${this.id}-prefix">Honorific Prefixes</label>
            <input type="text" id="${this.id}-prefix" value="${prefixes}" />
        </div><div class="input-group" title="Honorific Suffixes">
            <label for="${this.id}-suffix">Honorific Suffixes(s)</label>
            <input type="text" id="${this.id}-suffix" value="${suffixes}" />
        </div>`;
        e.insertAdjacentHTML('beforeend', html);
    }

    override loadValue() {
        const f = document.getElementById(this.id + '-surnames') as HTMLInputElement;
        const g = document.getElementById(this.id + '-given') as HTMLInputElement;
        const a = document.getElementById(this.id + '-additional') as HTMLInputElement;
        const p = document.getElementById(this.id + '-prefix') as HTMLInputElement;
        const s = document.getElementById(this.id + '-suffix') as HTMLInputElement;
        this.property.value = [f, g, a, p, s]
            .map(e => e.value.trim().split(' ').join(','))
            .join(';');
    }
}

export class CustomAddressInput extends CustomInput {
    override validate() {
        return null;
    }
    override html(e: HTMLElement) {
        /*
            The structured type value consists of a sequence of
            address components.  The component values MUST be specified in
            their corresponding position.  The structured type value
            corresponds, in sequence, to
                the post office box;
                the extended address (e.g., apartment or suite number);
                the street address;
                the locality (e.g., city);
                the region (e.g., state or province);
                the postal code;
                the country name (full name in the language specified in
                Section 5.1).

            For compatibility reasons the first 2 fields should be empty
        */

        const v = this.property.value || ';;;;;;;;';
        const components = v.split(';').map(s => s.trim());
        const poBox = components[0] || '';
        const extAddr = components[1] || '';
        const streetAddr = components[2] || '';
        const city = components[3] || '';
        const region = components[4] || '';
        const postCode = components[5] || '';
        const country = components[6] || '';

        // These 2 fields are deprecated so hidden by default
        const html =`<div class="input-group" ${poBox.length === 0 ? 'hidden' : ''
        } title="Post Office Box information">
            <label for="${this.id}-pobox">Post Office Box</label>
            <input type="text" id="${this.id}-pobox" value="${poBox}" />
        </div><div class="input-group" ${extAddr.length === 0 ? 'hidden' : ''
        } title="Extended address information (e.g., apartment or suite number)">
            <label for="${this.id}-extaddr">Extended Address</label>
            <input type="text" id="${this.id}-extaddr" value="${extAddr}" />
        </div><div class="input-group" title="Street Address">
            <label for="${this.id}-street">Street Address</label>
            <input type="text" id="${this.id}-street" value="${streetAddr}" />
        </div><div class="input-group" title="Locality (e.g., city)">
            <label for="${this.id}-city">City</label>
            <input type="text" id="${this.id}-city" value="${city}" />
        </div><div class="input-group" title="Region (e.g., state or province)">
            <label for="${this.id}-region">State/Province</label>
            <input type="text" id="${this.id}-region" value="${region}" />
        </div><div class="input-group" title="Postal code (e.g., zip code)">
            <label for="${this.id}-zip">Postal code</label>
            <input type="text" id="${this.id}-zip" value="${postCode}" />
        </div><div class="input-group" title="Country name">
            <label for="${this.id}-country">Country</label>
            <input type="text" id="${this.id}-country" value="${country}" />
        </div>`;
        e.insertAdjacentHTML('beforeend', html);
    }

    override loadValue() {
        this.property.value = [
            'pobox',
            'extaddr',
            'street',
            'city',
            'region',
            'zip',
            'country'
        ]
            .map(f => this.id + '-' + f)
            .map(f => document.getElementById(f) as HTMLInputElement)
            .map(e => e.value.trim())
            .join(';');
    }
}

export class CustomGeoInput extends CustomUriInput {
    override html(e: HTMLElement) {
        e.insertAdjacentHTML('beforeend',
            `<div class="input-group" title="You can use any URI here but the geo uri is a great fit">
<label for="${this.id}">Location</label>
<input type="url" id="${this.id}" placeholder="geo:41.878860,-87.635950" />
</div>`);
    }

    override validate(): string | null {
        const v = this.getValue();
        if (v.startsWith('geo:')) {
            const coords = v.slice(4).split('?')[0]?.split(',');
            if (coords.length < 2)
                return "a geo link is invalid if it does not contain both a latitude and longitude component";
            if (isNaN(Number(coords[0])))
                return "geo link contains invalid latitude component";
            if (isNaN(Number(coords[1])))
                return "geo link contains invalid longitude component";
        }
        return null;
    }

    override loadValue() {
        this.property.value = this.getValue();
    }
}

export class CustomImageInput extends CustomInput {

    // Android limit: 256x256
    // TODO 400x400 would be better for web
    static readonly MAX_DIM_PIXELS = 256;

    override validate(): string | null {
        const inp = document.getElementById(this.id) as HTMLInputElement;
        if (inp.files.length === 0 && !this.property.value)
            return 'No image provided';
        return null;
    }

    override html(e: HTMLElement) {
        e.insertAdjacentHTML('beforeend', `
        <div class="input-group">
        <label for="${this.id}">${this.property.label()}</label>
        <input type="file" id="${this.id}" accept="image/*" />
</div><span id="${this.id}-preview">${
            this.property.value
                ? `<fieldset><legend>Preview</legend>${this.property.valueHtml()}</fieldset>`
                : ''}</span>`);

        document.getElementById(this.id).addEventListener('change', async e => {
            const preview = document.getElementById(this.id + '-preview');
            try {
                preview.innerHTML = `<fieldset><legend>Preview</legend><img src="${
                    await this.resizedDataUrl()}" alt="failed to load image"></fieldset>`;
            } catch (e) {
                preview.innerHTML = "failed to load image";
            }
        });
    }

    protected async rawDataUrl(): Promise<string> {
        return new Promise((resolve, reject) => {
            const inp = document.getElementById(this.id) as HTMLInputElement;

            if (inp.files.length === 0)
                return reject(new Error('No file provided'));

            const fr = new FileReader();
            fr.onload = () => resolve(fr.result as string);
            fr.onerror = reject;
            fr.readAsDataURL(inp.files[0]);
        });
    }

    async resizedDataUrl(): Promise<string> {
        const url = await this.rawDataUrl();

        return new Promise((resolve, reject) => {
            const img = new Image();
            img.src = url;
            img.onerror = function (e) {
                URL.revokeObjectURL(this.src);
                reject(e);
            };
            img.onload = function () {
                // Calculate dimensions needed to truncate bottom/right excess of image
                let canvasDims: number, ih: number, iw: number;
                if (img.height > img.width) { // truncate bottom excess
                    iw = canvasDims = Math.min(img.width, CustomImageInput.MAX_DIM_PIXELS);
                    ih = img.height * canvasDims / img.width;
                } else { // height <= width
                    ih = canvasDims = Math.min(img.height, CustomImageInput.MAX_DIM_PIXELS);
                    iw = img.width * canvasDims / img.height;
                }

                // Put image into canvas with calculated dimensions
                const canvas = document.createElement('canvas');
                canvas.height = canvas.width = canvasDims;
                const ctx = canvas.getContext('2d');
                ctx.drawImage(img, 0, 0, iw, ih);

                // Extract dataurl from canvas
                let ret = canvas.toDataURL('image/jpeg', 1);
                if (ret.length > 200_000)
                    ret = canvas.toDataURL('image/jpeg', 0.7);
                if (ret.length > 200_000) {
                    console.warn("Scaled image still too big, reducing quality");
                    ret = canvas.toDataURL('image/jpeg', 0.5);
                }
                resolve(ret);
            };
        });
    }

    override async loadValue() {
        try {
            this.property.value = await this.resizedDataUrl();
        } catch (e) {
            console.error(e);
        }
    }
}

export class CustomTimezoneInput extends CustomInput {
    static tzDbOptions: string = null;

    isTzDbIdentifier: boolean = true;

    static async getTzDbOptions() {
        if (CustomTimezoneInput.tzDbOptions)
            return CustomTimezoneInput.tzDbOptions;

        const tzdb = await getTzDb();
        const formatOffset = (secs: number) =>
            secs == 0
            ? 'UTC'
            : `UTC${secs > 0 ? '+' : '-'
                }${Math.floor(Math.abs(secs) / (60 * 60))
                }${('0'+ Math.floor((Math.abs(secs) / 60) % 60)).slice(-2)}`;
        const options: [string, string][] = Object.entries(tzdb.zones)
            .map(([z, [offset, abbrev]]) => [
                    z,
                    z + (abbrev ? ` (${abbrev})`
                        : offset ? ` (${formatOffset(offset)})`
                            : '' )
                ] as [string, string])
            .concat(
                tzdb.links.map(([name, target]) =>
                    [name, name + (target ? ` (${target})` : '')]
                )
            ).sort((a, b) => a[0].localeCompare(b[0]));
        return CustomTimezoneInput.tzDbOptions = options.map(([value, text]) =>
            `<option value="${value}">${text}</option>`).join('');
    }

    getDefaultValue() {
        // UTC offset value
        if (['+', '-'].includes(this.property.value[0])) {
            this.isTzDbIdentifier = false;
            return [
                this.property.value[0],
                ...this.property.value.slice(1).split(':'),
                '00'
            ];
        }

        // No value
        if (!this.property.value) {
            this.isTzDbIdentifier = true;
            try {
                return Intl.DateTimeFormat().resolvedOptions().timeZone || 'UTC';
            } catch (_) {
                return 'UTC';
            }
        }
        // if (!this.property.value) {
        //     this.isTzDbIdentifier = false;
        //     const o = new Date().getTimezoneOffset();
        //     const sign = o < 0 ? '+' : '-'; // For some reason negated
        //     const hours = Math.floor(Math.abs(o) / 60);
        //     const mins = ('0' + Math.floor(Math.abs(o) % 60)).slice(-2);
        //     return [sign, hours, mins];
        // }

        // TZ database value
        this.isTzDbIdentifier = true;
        return this.property.value;
    }

    override validate(): string | null {
        if (this.isTzDbIdentifier)
            return null;
        return null;
    }

    override html(e: HTMLElement): void {
        const v = this.getDefaultValue();
        if (typeof v == "string") {
            e.insertAdjacentHTML('beforeend',
                `<div class="input-group"><label for="${this.id}">Timezone</label>
<select id="${this.id}"><option value="${v}" selected>${v}</option>${CustomTimezoneInput.tzDbOptions || ''}</select>`);
            if (!CustomTimezoneInput.tzDbOptions)
                CustomTimezoneInput.getTzDbOptions().then(() => {
                    document.getElementById(this.id).insertAdjacentHTML('beforeend',
                        CustomTimezoneInput.tzDbOptions)
                }).catch(console.error);

        } else {
            const signSelectHtml = `<select id="${this.id}-sign">${
                ['+','-'].map(s => `<option value="${s}"${v[0] === s ? ' selected' : ''}>${s}</option>`)
            }</select>`;
            e.insertAdjacentHTML('beforeend',
                `<div class="input-group"><label>Timezone</label><span>UTC${
                signSelectHtml}<input type="number" min="0" max="24" id="${
                this.id}-hours" value="${v[1] || '0'}">:<input type="number" id="${
                this.id}-mins" min="0" max="60" value="${v[2] || '0'}"></span></div>`);
        }
    }

    override loadValue() {
        if (this.isTzDbIdentifier) {
            const e = document.getElementById(this.id) as HTMLInputElement;
            this.property.value = e.value;
        } else {
            const sign = document.getElementById(this.id + '-sign') as HTMLInputElement;
            const hours = document.getElementById(this.id + '-hours') as HTMLInputElement;
            const mins = document.getElementById(this.id + '-mins') as HTMLInputElement;
            this.property.value = sign.value + hours.value + ':' + ('0' + mins).slice(-2);
        }
    }
}

export class CustomLangInput extends CustomTextInput {
    // TODO searchable dropdown
    placeholderText(): string {
        return 'en-US';
    }
    defaultValue(): string {
        return super.defaultValue() || navigator.language || '';
    }
}

export class CustomFiyUserInput extends CustomTextInput {
    override placeholderText(){
        return 'user@example.com';
    }

    override getValue() {
        const e = document.getElementById(this.id) as HTMLInputElement;
        if (!e.value.includes('@'))
            e.value += '@' + domain;
        return e.value;
    }

    override validate(): string | null {
        if (this.getValue().length === 0)
            return 'Value cannot be empty';
        if (this.getValue()[0] === '@')
            return 'User should not start with @';
        return null;
    }
}