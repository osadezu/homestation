// htmlContents.h
// File contains templates for HTML page bodies

// HTML source for main landing page
const char HTML_ROOT[] PROGMEM = R"=-=(
  <!DOCTYPE html>
  <html>
    <head>
      <title>Homestation</title>
    </head>
    <body>

      <h1>Conditions in {{TKN_RM_LABEL}}:</h1>
      <p>Humidity: {{TKN_HUMIDITY}} &percnt;<br>
         Temperature: {{TKN_TEMP_C}}&deg;C ({{TKN_TEMP_F}}&deg;F)</p>

      <p>Logged: {{TKN_AGE}} ago.</p>

    </body>
  </html>
)=-=";

// HTML source for Not Found page
const char HTML_NOTFOUND[] PROGMEM = R"=-=(
  <!DOCTYPE html>
  <html>
    <head>
      <title>Homestation</title>
    </head>
    <body>

      <h1>404</h1>
      <p>Page Not Found!</p>

    </body>
  </html>
)=-=";

// TODO: Break up HTML in header, body, footer to reuse parts and reduce used memory.
