from django.shortcuts import render, get_object_or_404, redirect
from .forms import LocationForm
from django.contrib.auth.models import User
from .models import Location
# Create your views here.
def create(request, user_id):
    user = get_object_or_404(User, pk=user_id)
    if request.method == 'POST':
        form = LocationForm(request.POST)
        if form.is_valid:
            location = form.save(commit=False)
            location.user = user
            location.save()
            return redirect('accounts:profile', user_id=user_id)

    else:
        form = LocationForm()
    return render(request, 'locations/location_form.html', {'form':form})

def edit(request, user_id, location_id):
    user = get_object_or_404(User, pk=user_id)
    location = get_object_or_404(Location, pk=location_id)
    if request.method == 'POST':
        form = LocationForm(request.POST, instance=location)
        if form.is_valid:
            location = form.save(commit=False)
            location.user = user
            location.save()
            return redirect('accounts:profile', user_id=user_id)

    else:
        form = LocationForm(instance=location)
    return render(request, 'locations/location_form.html', {'form':form})
