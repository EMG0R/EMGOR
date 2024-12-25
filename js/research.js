document.getElementById('research-form').addEventListener('submit', async function (e) {
    e.preventDefault();

    const responses = {
        question1: document.getElementById('question1').value,
        question2: document.getElementById('question2').value,
        question3: document.getElementById('question3').value,
        question4: document.getElementById('question4').value,
        question5: document.getElementById('question5').value,
    };

    console.log('Survey Responses:', responses);

    try {
        // Send the responses to the Netlify function
        const response = await fetch('/.netlify/functions/log-responses', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
            },
            body: JSON.stringify(responses),
        });

        if (response.ok) {
            alert('Thank you for your feedback!');
            e.target.reset();
        } else {
            const error = await response.json();
            console.error('Server Error:', error);
            alert('Failed to save responses. Please try again later.');
        }
    } catch (error) {
        console.error('Client Error:', error);
        alert('Failed to save responses. Please check your internet connection.');
    }
});