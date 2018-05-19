from django.conf.urls import url
from django.urls import path
from . import views

app_name = 'locations'

urlpatterns = [
    path('<int:user_id>/create/', views.create, name='create'),
    path('<int:user_id>/edit/<int:location_id>', views.edit, name='edit'),


]
