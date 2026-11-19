import VC from './VC';
import * as API from './api';
import {VCProperty} from "./VCProperty";

const contactList = document.getElementById("contact-list");
const contactDetails = document.getElementById("contact-details");
const searchInput = document.getElementById("search") as HTMLInputElement;
const sidebar = document.querySelector(".sidebar");
const detailsView = document.querySelector(".details-view");

let contacts: VC[];
let username: string;
let profileIndex: number;

API.getUserContacts().then(vcf => {
    // Parse contacts
    contacts = VC.parseCards(vcf);
    contacts.sort((a,b) => a.sortCompare(b));

    // Get profile
    for (let i = 0; i < contacts.length; i++) {
        const un = contacts[i].isProfileCard();
        if (typeof un === "string") {
            profileIndex = i;
            username = un;
        }
    }

    // Initial render
    const queryParams = new URL(document.location.toString()).searchParams;
    renderContactsList(queryParams.get('q') || "");

    if (queryParams.get('id')) {
        // Show contact by id
        const index = contacts.findIndex(c => c.getId() == queryParams.get('id'));
        showContactDetails(index);
    } else if (queryParams.get('user')) {
        const index = contacts.findIndex(c => c.getFiyUser() == queryParams.get('user'));
        showContactDetails(index);
    } else {
        // On mobile, user may want to see list of contacts
        if (window.innerWidth <= 768 && typeof queryParams.get('profile') !== 'string')
            return;

        // Show user profile editable
        if (profileIndex >= 0)
            showContactDetails(profileIndex);
        else
            contactDetails.innerHTML = 'Select a contact or click "+" to make a new one.';
    }
}).catch(e => {
    console.error(e);
    alert('failed to load contacts from the server!!');
});

function renderContactsList(filter?: string) {
    if (!filter) {
        filter = searchInput.value;
        if (!filter)
            filter = new URL(document.location.toString())
                .searchParams.get('q') || '';
    }

    contactList.innerHTML = '';
    contacts
        .filter(c => !filter
            || c.displayName().toLowerCase().includes(filter.toLowerCase()))
        .forEach((contact, index) => {
            // New contact entry
            const li = document.createElement("li");
            li.title = contact.displayName();
            li.onclick = () => showContactDetails(index);

            // PFP
            const img = document.createElement("img");
            img.src = contact.profilePhotoUri();
            img.alt = '[#]';
            img.className = 'pfp';
            img.onerror = e => { img.src = VC.defaultPfp; };
            li.appendChild(img);

            // Display name
            li.insertAdjacentHTML('beforeend', ' ' + contact.displayName());
            contactList.appendChild(li);
        });
}

function mobileDetailsView(goBack = false) {
    if (window.innerWidth > 768)
        return;

    if (goBack) {
        sidebar.classList.remove("hidden");
        detailsView.classList.remove("active");
    } else {
        sidebar.classList.add("hidden");
        detailsView.classList.add("active");
    }
}

// TODO accept reference instead of index, apply to all other functions here
function showContactDetails(index: number) {
    if (index === -1)
        index = profileIndex;
    contactDetails.innerHTML = `${contacts[index].showHtml()}<hr>
<!--<button title="Share"><i class="fa fa-share" id="contact-share"></i></button>-->
<button title="Download" id="contact-download"><i class="fa fa-download"></i></button>
<button title="Edit" id="contact-edit"><i class="fa fa-pencil"></i></button>
<button title="Delete" id="contact-delete"><i class="fa fa-trash"></i></button>`;

    // document.getElementById('contact-share').addEventListener('click', e => {
    //     alert("not yet implemented");
    // });
    document.getElementById('contact-download').addEventListener('click', e => {
        contacts[index].download();
    });
    document.getElementById('contact-edit').addEventListener('click', e => {
        editContact(index);
    });
    document.getElementById('contact-delete').addEventListener('click', e => {
        if (contacts[index].isProfileCard()) {
            alert('Cannot delete profile contact, please edit it instead');
            return;
        }
        API.deleteContact(contacts[index])
            .then(() => {
                if (profileIndex >= 0)
                    showContactDetails(profileIndex);
                else
                    contactDetails.innerHTML = 'Select a contact or click "+" to create a new one.';
            }).catch(e => {
                console.error(e);
                alert('Failed to delete contact from the server. Try again later.');
            });
    });

    mobileDetailsView();
}

function editContact(index: number) {
    contactDetails.innerHTML = '';
    const form = document.createElement('div');
    contactDetails.appendChild(form);
    contacts[index].addEditHtml(form);
    contactDetails.insertAdjacentHTML(
        'beforeend',
        '<hr><span id="edit-contact-errs"></span>'
        + '<button id="edit-contact-save"><i class="fa fa-check"></i> Save</button>'
        + '<button id="edit-contact-cancel"><i class="fa fa-remove"></i> Cancel</button>');

    // Save contact edits
    document.getElementById('edit-contact-save').addEventListener('click', async event => {
        // Validate
        const errs = contacts[index].validationErrors();
        const el = document.getElementById('edit-contact-errs');
        if (errs.length > 0) {
            el.innerHTML = `<span class="validation-errors">Please fix the following issues: <ul>${
                errs.map(e => `<li>${e}</li>`).join('')
            }</ul>`;
            return;
        }
        el.innerHTML = '';

        // Update local
        await contacts[index].acceptEdits();

        // Update remote
        API.updateContact(contacts[index]).then((newVcText) => {
            contacts[index] = VC.parseCard(newVcText);
            showContactDetails(index);
            renderContactsList();
        }).catch(e => {
            console.error(e);
            alert('Failed to update contact, try again later.');
        });
    });

    document.getElementById('edit-contact-cancel').addEventListener('click', e => {
        showContactDetails(index);
    });

    mobileDetailsView();
}

function newContact() {
    const index = contacts.length;
    contacts.push(new VC());
    contacts[index].isUnsaved = true;
    contacts[index].properties.push(new VCProperty('VERSION', {}, '4.0'));
    contacts[index].properties.push(new VCProperty('FN')); // All contacts must have a name
    editContact(index);
}

searchInput.addEventListener("input", () => {
    renderContactsList(searchInput.value);
});

const fileInp = document.getElementById('contact-upload') as HTMLInputElement;
fileInp.addEventListener('change', async e => {

    // TODO allow multi-imports
    const text = await fileInp.files[0].text();
    const vcs = VC.parseCards(text);
    const reqs = vcs.map(
        c => API.updateContact(c.convertInternal())
    ).join('\n');
    try {
        await Promise.all(reqs);
        window.location.reload();
    } catch (e) {
        console.error(e);
        alert('Import failed, reload the page');
    }
});

globalThis.newContact = newContact;
globalThis.mobileDetailsView = mobileDetailsView;
globalThis.showContactDetails = showContactDetails;
globalThis.uploadContact = () => fileInp.click();