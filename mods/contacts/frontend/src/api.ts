//
// API for communicating with the backend
//

import VC from "./VC";

export const base_uri = "{{fiy_contacts_base_uri}}";
export const domain = "{{fiy_contacts_domain}}";

export async function getUserContacts() {
    // Just for now while I work on frontend
    const r = await fetch(base_uri + '/all');
    return await r.text();
}

export async function importSafeVcfString(vcf: string) {
    const r = await fetch(base_uri + '/save', {
        method: 'POST',
        body: vcf,
    });
    return r.text();
}

export async function updateContact(contact: VC) {
    return await importSafeVcfString(contact.vCardString());
}

export async function deleteContact(contact: VC) {
    const r = await fetch(base_uri + '/delete/' + contact.getId(), {
        method: 'DELETE',
    });
    return r.text();
}

export interface TzDb {
    version: string,
    zones: { [k: string]: [number, string] },
    links: [string, string][],
}

export async function getTzDb(): Promise<TzDb> {
    const r = await fetch(base_uri + '/tzdb');
    return await r.json();
}
