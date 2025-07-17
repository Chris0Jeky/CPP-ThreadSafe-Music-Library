// Audio Library Web GUI
// Main application logic

// State management
let library = [];
let currentView = 'library';
let sortBy = 'title';
let searchResults = [];

// Initialize application
document.addEventListener('DOMContentLoaded', () => {
    initializeEventListeners();
    loadLibraryFromStorage();
    showView('library');
    updateStatistics();
});

// Event Listeners
function initializeEventListeners() {
    // Navigation
    document.querySelectorAll('.nav-item').forEach(item => {
        item.addEventListener('click', (e) => {
            e.preventDefault();
            const view = item.dataset.view;
            showView(view);
        });
    });

    // Add track form
    document.getElementById('add-track-form').addEventListener('submit', (e) => {
        e.preventDefault();
        addTrack();
    });

    // Edit track form
    document.getElementById('edit-track-form').addEventListener('submit', (e) => {
        e.preventDefault();
        saveEditedTrack();
    });

    // Sort select
    document.getElementById('sort-select').addEventListener('change', (e) => {
        sortBy = e.target.value;
        displayTracks();
    });

    // Search
    document.getElementById('search-btn').addEventListener('click', performSearch);
    document.getElementById('search-input').addEventListener('keypress', (e) => {
        if (e.key === 'Enter') performSearch();
    });

    // Import/Export
    document.getElementById('file-input').addEventListener('change', handleFileSelect);
    document.getElementById('import-btn').addEventListener('click', importFile);
    document.getElementById('export-csv').addEventListener('click', () => exportLibrary('csv'));
    document.getElementById('export-json').addEventListener('click', () => exportLibrary('json'));
    document.getElementById('export-current').addEventListener('click', exportCurrentView);
    document.getElementById('load-sample').addEventListener('click', loadSampleData);

    // Modal close
    document.querySelector('.close').addEventListener('click', closeEditModal);
    window.addEventListener('click', (e) => {
        if (e.target.id === 'edit-modal') closeEditModal();
    });
}

// View Management
function showView(viewName) {
    document.querySelectorAll('.view').forEach(view => {
        view.classList.remove('active');
    });
    document.querySelectorAll('.nav-item').forEach(item => {
        item.classList.remove('active');
    });

    document.getElementById(`${viewName}-view`).classList.add('active');
    document.querySelector(`[data-view="${viewName}"]`).classList.add('active');
    currentView = viewName;

    if (viewName === 'library') {
        displayTracks();
    } else if (viewName === 'stats') {
        updateStatistics();
    }
}

// Track Management
function addTrack() {
    const track = {
        title: document.getElementById('track-title').value,
        artist: document.getElementById('track-artist').value,
        duration: parseInt(document.getElementById('track-duration').value),
        album: document.getElementById('track-album').value || '',
        genre: document.getElementById('track-genre').value || '',
        year: parseInt(document.getElementById('track-year').value) || 0,
        rating: parseFloat(document.getElementById('track-rating').value) || 0,
        playCount: parseInt(document.getElementById('track-playcount').value) || 0
    };

    // Validate
    if (!track.title || !track.artist || !track.duration) {
        showToast('Please fill in all required fields', 'error');
        return;
    }

    // Check for duplicates
    if (library.some(t => t.title === track.title && t.artist === track.artist)) {
        showToast('This track already exists in the library', 'error');
        return;
    }

    library.push(track);
    saveLibraryToStorage();
    document.getElementById('add-track-form').reset();
    showToast('Track added successfully!', 'success');
    
    if (currentView === 'library') {
        displayTracks();
    }
}

function deleteTrack(title, artist) {
    if (!confirm(`Are you sure you want to delete "${title}" by ${artist}?`)) {
        return;
    }

    library = library.filter(t => !(t.title === title && t.artist === artist));
    saveLibraryToStorage();
    displayTracks();
    showToast('Track deleted successfully', 'info');
}

function editTrack(title, artist) {
    const track = library.find(t => t.title === title && t.artist === artist);
    if (!track) return;

    // Populate edit form
    document.getElementById('edit-original-title').value = track.title;
    document.getElementById('edit-original-artist').value = track.artist;
    document.getElementById('edit-title').value = track.title;
    document.getElementById('edit-artist').value = track.artist;
    document.getElementById('edit-duration').value = track.duration;
    document.getElementById('edit-album').value = track.album;
    document.getElementById('edit-genre').value = track.genre;
    document.getElementById('edit-year').value = track.year || '';
    document.getElementById('edit-rating').value = track.rating || '';
    document.getElementById('edit-playcount').value = track.playCount || '';

    // Show modal
    document.getElementById('edit-modal').style.display = 'block';
}

function saveEditedTrack() {
    const originalTitle = document.getElementById('edit-original-title').value;
    const originalArtist = document.getElementById('edit-original-artist').value;
    
    const trackIndex = library.findIndex(t => t.title === originalTitle && t.artist === originalArtist);
    if (trackIndex === -1) return;

    const updatedTrack = {
        title: document.getElementById('edit-title').value,
        artist: document.getElementById('edit-artist').value,
        duration: parseInt(document.getElementById('edit-duration').value),
        album: document.getElementById('edit-album').value || '',
        genre: document.getElementById('edit-genre').value || '',
        year: parseInt(document.getElementById('edit-year').value) || 0,
        rating: parseFloat(document.getElementById('edit-rating').value) || 0,
        playCount: parseInt(document.getElementById('edit-playcount').value) || 0
    };

    library[trackIndex] = updatedTrack;
    saveLibraryToStorage();
    closeEditModal();
    displayTracks();
    showToast('Track updated successfully!', 'success');
}

function closeEditModal() {
    document.getElementById('edit-modal').style.display = 'none';
}

// Display Functions
function displayTracks(tracks = library) {
    const tbody = document.getElementById('tracks-tbody');
    const emptyState = document.getElementById('empty-library');
    
    if (tracks.length === 0) {
        tbody.innerHTML = '';
        emptyState.style.display = 'block';
        return;
    }
    
    emptyState.style.display = 'none';
    
    // Sort tracks
    const sortedTracks = [...tracks].sort((a, b) => {
        switch (sortBy) {
            case 'title': return a.title.localeCompare(b.title);
            case 'artist': return a.artist.localeCompare(b.artist);
            case 'album': return a.album.localeCompare(b.album);
            case 'year': return b.year - a.year;
            case 'rating': return b.rating - a.rating;
            case 'playcount': return b.playCount - a.playCount;
            default: return 0;
        }
    });
    
    tbody.innerHTML = sortedTracks.map(track => `
        <tr>
            <td>${escapeHtml(track.title)}</td>
            <td>${escapeHtml(track.artist)}</td>
            <td>${escapeHtml(track.album)}</td>
            <td>${formatDuration(track.duration)}</td>
            <td><span class="genre-badge">${escapeHtml(track.genre)}</span></td>
            <td>${track.year || '-'}</td>
            <td>${formatRating(track.rating)}</td>
            <td>${track.playCount || 0}</td>
            <td>
                <div class="track-actions">
                    <button class="action-btn edit-btn" onclick="editTrack('${escapeHtml(track.title)}', '${escapeHtml(track.artist)}')">
                        <i class="fas fa-edit"></i>
                    </button>
                    <button class="action-btn delete-btn" onclick="deleteTrack('${escapeHtml(track.title)}', '${escapeHtml(track.artist)}')">
                        <i class="fas fa-trash"></i>
                    </button>
                </div>
            </td>
        </tr>
    `).join('');
}

// Search Function
function performSearch() {
    const query = document.getElementById('search-input').value.trim();
    if (!query) {
        showToast('Please enter a search query', 'error');
        return;
    }

    const mode = document.getElementById('search-mode').value;
    const caseSensitive = document.getElementById('case-sensitive').checked;
    const searchFields = {
        title: document.getElementById('search-title').checked,
        artist: document.getElementById('search-artist').checked,
        album: document.getElementById('search-album').checked,
        genre: document.getElementById('search-genre').checked
    };

    // Perform search based on mode
    searchResults = library.filter(track => {
        const fields = [];
        if (searchFields.title) fields.push(track.title);
        if (searchFields.artist) fields.push(track.artist);
        if (searchFields.album) fields.push(track.album);
        if (searchFields.genre) fields.push(track.genre);

        return fields.some(field => {
            if (!field) return false;
            const fieldValue = caseSensitive ? field : field.toLowerCase();
            const searchQuery = caseSensitive ? query : query.toLowerCase();

            switch (mode) {
                case 'exact':
                    return fieldValue === searchQuery;
                case 'prefix':
                    return fieldValue.startsWith(searchQuery);
                case 'fuzzy':
                    return fuzzyMatch(fieldValue, searchQuery);
                case 'regex':
                    try {
                        const regex = new RegExp(query, caseSensitive ? 'g' : 'gi');
                        return regex.test(field);
                    } catch (e) {
                        showToast('Invalid regular expression', 'error');
                        return false;
                    }
                case 'substring':
                default:
                    return fieldValue.includes(searchQuery);
            }
        });
    });

    // Display results
    const resultsDiv = document.getElementById('search-results');
    if (searchResults.length === 0) {
        resultsDiv.innerHTML = '<p class="empty-state">No tracks found matching your search.</p>';
    } else {
        resultsDiv.innerHTML = `
            <h3>Found ${searchResults.length} track${searchResults.length !== 1 ? 's' : ''}</h3>
            <div class="search-results-list">
                ${searchResults.map(track => `
                    <div class="list-item">
                        <div>
                            <strong>${escapeHtml(track.title)}</strong> by ${escapeHtml(track.artist)}
                            ${track.album ? `<br><small>${escapeHtml(track.album)}</small>` : ''}
                        </div>
                        <div class="track-actions">
                            <button class="action-btn edit-btn" onclick="editTrack('${escapeHtml(track.title)}', '${escapeHtml(track.artist)}')">
                                <i class="fas fa-edit"></i>
                            </button>
                            <button class="action-btn delete-btn" onclick="deleteTrack('${escapeHtml(track.title)}', '${escapeHtml(track.artist)}')">
                                <i class="fas fa-trash"></i>
                            </button>
                        </div>
                    </div>
                `).join('')}
            </div>
            <button class="btn btn-secondary" onclick="exportSearchResults()" style="margin-top: 1rem;">
                <i class="fas fa-download"></i> Export Results
            </button>
        `;
    }
}

// Import/Export Functions
function handleFileSelect(e) {
    const file = e.target.files[0];
    if (file) {
        document.getElementById('file-name').textContent = file.name;
        document.getElementById('import-btn').disabled = false;
    }
}

function importFile() {
    const fileInput = document.getElementById('file-input');
    const file = fileInput.files[0];
    if (!file) return;

    const reader = new FileReader();
    reader.onload = (e) => {
        try {
            const content = e.target.result;
            const extension = file.name.split('.').pop().toLowerCase();

            if (extension === 'csv') {
                importCSV(content);
            } else if (extension === 'json') {
                importJSON(content);
            } else {
                showToast('Unsupported file format', 'error');
            }
        } catch (error) {
            showToast('Error importing file: ' + error.message, 'error');
        }
    };
    reader.readAsText(file);
}

function importCSV(content) {
    const lines = content.trim().split('\n');
    if (lines.length < 2) {
        showToast('CSV file is empty or invalid', 'error');
        return;
    }

    // Skip header
    const imported = [];
    for (let i = 1; i < lines.length; i++) {
        const values = parseCSVLine(lines[i]);
        if (values.length >= 3) {
            const track = {
                title: values[0],
                artist: values[1],
                duration: parseInt(values[2]) || 0,
                album: values[3] || '',
                genre: values[4] || '',
                year: parseInt(values[5]) || 0,
                playCount: parseInt(values[6]) || 0,
                rating: parseFloat(values[7]) || 0
            };

            // Validate and check for duplicates
            if (track.title && track.artist && track.duration > 0) {
                if (!library.some(t => t.title === track.title && t.artist === track.artist)) {
                    imported.push(track);
                }
            }
        }
    }

    if (imported.length > 0) {
        library.push(...imported);
        saveLibraryToStorage();
        displayTracks();
        showToast(`Imported ${imported.length} tracks successfully!`, 'success');
    } else {
        showToast('No valid tracks found in file', 'error');
    }

    // Reset form
    document.getElementById('file-input').value = '';
    document.getElementById('file-name').textContent = 'No file selected';
    document.getElementById('import-btn').disabled = true;
}

function importJSON(content) {
    try {
        const data = JSON.parse(content);
        const tracks = Array.isArray(data) ? data : data.tracks || [];
        
        const imported = [];
        for (const track of tracks) {
            if (track.title && track.artist && track.duration > 0) {
                if (!library.some(t => t.title === track.title && t.artist === track.artist)) {
                    imported.push({
                        title: track.title,
                        artist: track.artist,
                        duration: track.duration,
                        album: track.album || '',
                        genre: track.genre || '',
                        year: track.year || 0,
                        playCount: track.playCount || track.play_count || 0,
                        rating: track.rating || 0
                    });
                }
            }
        }

        if (imported.length > 0) {
            library.push(...imported);
            saveLibraryToStorage();
            displayTracks();
            showToast(`Imported ${imported.length} tracks successfully!`, 'success');
        } else {
            showToast('No valid tracks found in file', 'error');
        }
    } catch (e) {
        showToast('Invalid JSON file', 'error');
    }

    // Reset form
    document.getElementById('file-input').value = '';
    document.getElementById('file-name').textContent = 'No file selected';
    document.getElementById('import-btn').disabled = true;
}

function exportLibrary(format) {
    if (library.length === 0) {
        showToast('Library is empty', 'error');
        return;
    }

    if (format === 'csv') {
        exportCSV(library, 'audio_library.csv');
    } else if (format === 'json') {
        exportJSON(library, 'audio_library.json');
    }
}

function exportCSV(tracks, filename) {
    const headers = 'Title,Artist,Duration,Album,Genre,Year,PlayCount,Rating';
    const rows = tracks.map(track => 
        [
            escapeCSV(track.title),
            escapeCSV(track.artist),
            track.duration,
            escapeCSV(track.album),
            escapeCSV(track.genre),
            track.year || '',
            track.playCount || 0,
            track.rating || 0
        ].join(',')
    );

    const csv = [headers, ...rows].join('\n');
    downloadFile(csv, filename, 'text/csv');
    showToast(`Exported ${tracks.length} tracks to CSV`, 'success');
}

function exportJSON(tracks, filename) {
    const json = JSON.stringify({ tracks }, null, 2);
    downloadFile(json, filename, 'application/json');
    showToast(`Exported ${tracks.length} tracks to JSON`, 'success');
}

function exportCurrentView() {
    const tracks = currentView === 'search' && searchResults.length > 0 ? searchResults : library;
    exportCSV(tracks, 'audio_library_export.csv');
}

function exportSearchResults() {
    if (searchResults.length === 0) return;
    exportCSV(searchResults, 'search_results.csv');
}

// Statistics
function updateStatistics() {
    // Basic stats
    document.getElementById('total-tracks').textContent = library.length;
    
    const artists = new Set(library.map(t => t.artist));
    document.getElementById('total-artists').textContent = artists.size;
    
    const albums = new Set(library.filter(t => t.album).map(t => t.album));
    document.getElementById('total-albums').textContent = albums.size;
    
    const totalDuration = library.reduce((sum, t) => sum + t.duration, 0);
    document.getElementById('total-duration').textContent = formatDuration(totalDuration);

    // Genre distribution
    const genreCount = {};
    library.forEach(track => {
        if (track.genre) {
            genreCount[track.genre] = (genreCount[track.genre] || 0) + 1;
        }
    });

    const genreChart = document.getElementById('genre-chart');
    const sortedGenres = Object.entries(genreCount)
        .sort((a, b) => b[1] - a[1])
        .slice(0, 5);

    genreChart.innerHTML = sortedGenres.map(([genre, count]) => `
        <div class="list-item">
            <span>${escapeHtml(genre)}</span>
            <span>${count} tracks</span>
        </div>
    `).join('');

    // Top rated tracks
    const topRated = [...library]
        .filter(t => t.rating > 0)
        .sort((a, b) => b.rating - a.rating)
        .slice(0, 5);

    document.getElementById('top-rated').innerHTML = topRated.map(track => `
        <div class="list-item">
            <div>
                <strong>${escapeHtml(track.title)}</strong><br>
                <small>${escapeHtml(track.artist)}</small>
            </div>
            <span class="rating-stars">${formatRating(track.rating)}</span>
        </div>
    `).join('');

    // Most played tracks
    const mostPlayed = [...library]
        .filter(t => t.playCount > 0)
        .sort((a, b) => b.playCount - a.playCount)
        .slice(0, 5);

    document.getElementById('most-played').innerHTML = mostPlayed.map(track => `
        <div class="list-item">
            <div>
                <strong>${escapeHtml(track.title)}</strong><br>
                <small>${escapeHtml(track.artist)}</small>
            </div>
            <span>${track.playCount} plays</span>
        </div>
    `).join('');
}

// Utility Functions
function formatDuration(seconds) {
    const hours = Math.floor(seconds / 3600);
    const minutes = Math.floor((seconds % 3600) / 60);
    const secs = seconds % 60;
    
    if (hours > 0) {
        return `${hours}:${minutes.toString().padStart(2, '0')}:${secs.toString().padStart(2, '0')}`;
    }
    return `${minutes}:${secs.toString().padStart(2, '0')}`;
}

function formatRating(rating) {
    if (!rating) return '-';
    const stars = Math.round(rating);
    return '★'.repeat(stars) + '☆'.repeat(5 - stars);
}

function escapeHtml(text) {
    const div = document.createElement('div');
    div.textContent = text;
    return div.innerHTML;
}

function escapeCSV(text) {
    if (text.includes(',') || text.includes('"') || text.includes('\n')) {
        return `"${text.replace(/"/g, '""')}"`;
    }
    return text;
}

function parseCSVLine(line) {
    const result = [];
    let current = '';
    let inQuotes = false;
    
    for (let i = 0; i < line.length; i++) {
        const char = line[i];
        const nextChar = line[i + 1];
        
        if (char === '"') {
            if (inQuotes && nextChar === '"') {
                current += '"';
                i++;
            } else {
                inQuotes = !inQuotes;
            }
        } else if (char === ',' && !inQuotes) {
            result.push(current);
            current = '';
        } else {
            current += char;
        }
    }
    
    result.push(current);
    return result;
}

function fuzzyMatch(str, pattern) {
    const strLower = str.toLowerCase();
    const patternLower = pattern.toLowerCase();
    let patternIdx = 0;
    
    for (let i = 0; i < strLower.length && patternIdx < patternLower.length; i++) {
        if (strLower[i] === patternLower[patternIdx]) {
            patternIdx++;
        }
    }
    
    return patternIdx === patternLower.length;
}

function downloadFile(content, filename, type) {
    const blob = new Blob([content], { type });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = filename;
    document.body.appendChild(a);
    a.click();
    document.body.removeChild(a);
    URL.revokeObjectURL(url);
}

function showToast(message, type = 'info') {
    const toast = document.createElement('div');
    toast.className = `toast ${type}`;
    toast.innerHTML = `
        <i class="fas fa-${type === 'success' ? 'check-circle' : type === 'error' ? 'exclamation-circle' : 'info-circle'}"></i>
        <span>${message}</span>
    `;
    
    document.getElementById('toast-container').appendChild(toast);
    
    setTimeout(() => {
        toast.remove();
    }, 3000);
}

// Local Storage
function saveLibraryToStorage() {
    localStorage.setItem('audioLibrary', JSON.stringify(library));
}

function loadLibraryFromStorage() {
    const stored = localStorage.getItem('audioLibrary');
    if (stored) {
        try {
            library = JSON.parse(stored);
        } catch (e) {
            library = [];
        }
    }
}

// Sample Data
function loadSampleData() {
    const sampleTracks = [
        { title: "Bohemian Rhapsody", artist: "Queen", duration: 354, album: "A Night at the Opera", genre: "Rock", year: 1975, rating: 5, playCount: 150 },
        { title: "Imagine", artist: "John Lennon", duration: 183, album: "Imagine", genre: "Rock", year: 1971, rating: 5, playCount: 120 },
        { title: "Hotel California", artist: "Eagles", duration: 391, album: "Hotel California", genre: "Rock", year: 1976, rating: 4.5, playCount: 98 },
        { title: "Stairway to Heaven", artist: "Led Zeppelin", duration: 482, album: "Led Zeppelin IV", genre: "Rock", year: 1971, rating: 5, playCount: 142 },
        { title: "Billie Jean", artist: "Michael Jackson", duration: 294, album: "Thriller", genre: "Pop", year: 1982, rating: 4.5, playCount: 89 },
        { title: "Like a Rolling Stone", artist: "Bob Dylan", duration: 369, album: "Highway 61 Revisited", genre: "Rock", year: 1965, rating: 4, playCount: 76 },
        { title: "Smells Like Teen Spirit", artist: "Nirvana", duration: 301, album: "Nevermind", genre: "Grunge", year: 1991, rating: 4.5, playCount: 110 },
        { title: "Hey Jude", artist: "The Beatles", duration: 431, album: "Hey Jude", genre: "Rock", year: 1968, rating: 5, playCount: 134 },
        { title: "One", artist: "U2", duration: 276, album: "Achtung Baby", genre: "Rock", year: 1991, rating: 4, playCount: 65 },
        { title: "Sweet Child O' Mine", artist: "Guns N' Roses", duration: 356, album: "Appetite for Destruction", genre: "Rock", year: 1987, rating: 4.5, playCount: 92 }
    ];

    library = sampleTracks;
    saveLibraryToStorage();
    displayTracks();
    updateStatistics();
    showToast('Sample data loaded successfully!', 'success');
}