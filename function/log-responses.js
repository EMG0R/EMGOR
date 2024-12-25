const fs = require('fs');
const path = require('path');

exports.handler = async function (event) {
    if (event.httpMethod !== 'POST') {
        return {
            statusCode: 405,
            body: JSON.stringify({ error: 'Method Not Allowed' }),
        };
    }

    try {
        const responses = JSON.parse(event.body);
        const logFilePath = path.resolve(__dirname, '../../survey-responses.json');

        // Read existing responses
        let logs = [];
        if (fs.existsSync(logFilePath)) {
            const data = fs.readFileSync(logFilePath, 'utf-8');
            logs = JSON.parse(data);
        }

        // Add the new response
        logs.push(responses);

        // Save responses back to file
        fs.writeFileSync(logFilePath, JSON.stringify(logs, null, 2));

        return {
            statusCode: 200,
            body: JSON.stringify({ message: 'Responses saved successfully!' }),
        };
    } catch (error) {
        console.error('Error saving responses:', error);
        return {
            statusCode: 500,
            body: JSON.stringify({ error: 'Failed to save responses.' }),
        };
    }
};