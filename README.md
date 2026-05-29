# MailGuy

A simple command-line utility for downloading all your Gmail as markdown files.

You'll need to generate a `client_secrets.json` via the Google Cloud Console (sigh) and then put it in the `build` folder.

When you run the app for the first time it will do the Google auth flow.
Then, the refresh token will be saved in your user data folder as `refresh_token.txt`.
As long as this file exists, you will not need to re-authenticate when starting the app.


## LICENSE

This software is dual-licensed to the public domain and under the following
license: you are granted a perpetual, irrevocable license to copy, modify,
publish, and distribute this file as you see fit.

## CREDITS

Written by Nick Aversano
Credits are much appreciated but not required.

Made for the [Handmade Network Essentials Jam](https://handmade.network/jam/essentials).