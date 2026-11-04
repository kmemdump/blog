const CONTACT_DETAILS = ['kmemdump', 'gmail.com'];

const contactForm = document.getElementById('contact-form');
const contactError = document.getElementById('contact-error');

function contactEmail() {
    return CONTACT_DETAILS.join('@');
}

function setFieldError(name, message) {
    const fieldError = document.querySelector(`[data-error-for="${name}"]`);

    if (!fieldError) {
        return;
    }

    if (message) {
        fieldError.textContent = message;
        fieldError.hidden = false;
    } else {
        fieldError.textContent = '';
        fieldError.hidden = true;
    }
}

function validateForm(values) {
    const errors = {};

    if (!values.name.trim()) {
        errors.name = 'Please enter your name.';
    }

    if (!values.email.trim()) {
        errors.email = 'Please enter your email.';
    } else if (!/^[^\s@]+@[^\s@]+\.[^\s@]+$/.test(values.email.trim())) {
        errors.email = 'Please enter a valid email address.';
    }

    if (!values.subject.trim()) {
        errors.subject = 'Please enter a subject.';
    }

    if (!values.message.trim()) {
        errors.message = 'Please enter a message.';
    }

    return errors;
}

function buildMailto(values) {
    const recipient = contactEmail();
    const name = values.name.trim();
    const email = values.email.trim();
    const subject = values.subject.trim();
    const message = values.message.trim();

    const subjectLine = `[src:blog.kmemdump.com/contact] ${subject}`;
    const body = `Name: ${name}\nEmail: ${email}\n\n${message}`;
    const mailto = `mailto:${recipient}?subject=${encodeURIComponent(subjectLine)}&body=${encodeURIComponent(body)}`;

    if (mailto.length > 1800) {
        throw new Error('mailto length');
    }

    return mailto;
}

function openMailClient(mailtoLink) {
    const link = document.createElement('a');
    link.href = mailtoLink;
    document.body.appendChild(link);
    link.click();
    link.remove();
}

function showEmailError(_) {
    if (!contactError) {
        return;
    }

    contactError.textContent = 'Could not send the email from this browser. Please try again or use the fallback below to open your email app.';
    contactError.hidden = false;
}

function hideEmailError() {
    if (contactError) {
        contactError.hidden = true;
        contactError.textContent = '';
    }
}

if (contactForm) {
    contactForm.addEventListener('submit', (event) => {
        event.preventDefault();

        const values = {
            name: contactForm.elements.name.value,
            email: contactForm.elements.email.value,
            subject: contactForm.elements.subject.value,
            message: contactForm.elements.message.value,
        };

        const errors = validateForm(values);

        for (const fieldName of ['name', 'email', 'subject', 'message']) {
            setFieldError(fieldName, errors[fieldName]);
        }

        if (Object.keys(errors).length > 0) {
            const firstInvalidField = Object.keys(errors)[0];
            contactForm.elements[firstInvalidField].focus();
            return;
        }

        hideEmailError();

        let mailtoLink;

        try {
            mailtoLink = buildMailto(values);
        } catch {
            showEmailError(values);
            return;
        }

        // Direct anchor click keeps this in the user-activation handler that
        // browsers require for opening the registered mail client.
        try {
            openMailClient(mailtoLink);
        } catch {

            setTimeout(() => {
                if (document.visibilityState === 'hidden') {
                    return;
                }

                showEmailError(values);
            }, 800);
        }
    });
}
