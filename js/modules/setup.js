window.SetupModule = {
  render: async function() {
    return `
      <div class="header">
        <h1 class="page-title">Welcome to Q-Manage</h1>
      </div>
      
      <div style="max-width: 600px; margin: 40px auto; background: var(--bg-card); border: 1px solid var(--border); border-radius: 12px; padding: 32px; box-shadow: 0 4px 12px rgba(0,0,0,0.05);">
        <h2 style="font-size: 1.5rem; font-weight: 600; margin-bottom: 8px;">Let's set up your organization</h2>
        <p style="color: var(--text-muted); margin-bottom: 24px;">Please provide some basic details so we can configure your language, currency, and localization preferences automatically.</p>
        
        <div class="form-group">
          <label class="form-label required">Organization / Your Name</label>
          <input type="text" id="setup-name" class="form-control" placeholder="Acme Corp" required>
        </div>
        
        <div class="form-group" style="margin-top: 16px;">
          <label class="form-label required">Country</label>
          <select id="setup-country" class="form-control" required>
            <option value="" disabled selected>Select a country</option>
            <option value="IN">India</option>
            <option value="US">United States</option>
            <option value="UK">United Kingdom</option>
            <option value="AE">United Arab Emirates</option>
            <option value="SG">Singapore</option>
            <option value="AU">Australia</option>
            <option value="OTHER">Other</option>
          </select>
        </div>

        <div class="form-group" style="margin-top: 16px;">
          <label class="form-label">Primary Industry</label>
          <select id="setup-industry" class="form-control">
            <option value="services">Services & Consulting</option>
            <option value="retail">Retail & E-commerce</option>
            <option value="manufacturing">Manufacturing</option>
            <option value="freelance">Freelance / Individual</option>
            <option value="other">Other</option>
          </select>
        </div>

        <div style="margin-top: 32px; display: flex; justify-content: flex-end;">
          <button class="btn btn-primary" onclick="SetupModule.completeSetup()">Complete Setup &rarr;</button>
        </div>
      </div>
    `;
  },

  completeSetup: async function() {
    const name = document.getElementById('setup-name').value.trim();
    const country = document.getElementById('setup-country').value;
    
    if (!name || !country) {
      QManage.toast('Please fill in your name and country.', 'error');
      return;
    }

    try {
      // Create default settings based on country
      let currency = 'INR';
      let language = 'en-IN';
      
      switch(country) {
        case 'US': currency = 'USD'; language = 'en-US'; break;
        case 'UK': currency = 'GBP'; language = 'en-GB'; break;
        case 'AE': currency = 'AED'; language = 'en-AE'; break;
        case 'SG': currency = 'SGD'; language = 'en-SG'; break;
        case 'AU': currency = 'AUD'; language = 'en-AU'; break;
        default: currency = 'INR'; language = 'en-IN'; break;
      }

      // Save to localStorage (simulate backend settings save for now, or use API if available)
      localStorage.setItem('qm_org_name', name);
      localStorage.setItem('qm_org_country', country);
      localStorage.setItem('qm_currency', currency);
      localStorage.setItem('qm_language', language);
      localStorage.setItem('qm_setup_complete', 'true');

      // Update basic settings object if possible
      // await API.put('/settings', { company_name: name, currency: currency, fiscal_year_start: 'april' });

      QManage.toast('Setup complete! Welcome to Q-Manage.', 'success');
      
      // Redirect to dashboard
      window.location.hash = '#/dashboard';
      window.location.reload(); // Reload to apply localization everywhere
    } catch (err) {
      console.error(err);
      QManage.toast('Failed to save setup data.', 'error');
    }
  }
};
